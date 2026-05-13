/*
 * ShizukuDirectLoader
 *
 * Third privileged-shell route for MiuiserPeruser.
 *
 * Unlike `rish` (which calls rikka.shizuku.shell.ShizukuShellLoader and wraps
 * every command in a fresh JVM), this loader is intended to be started ONCE by
 * the daemon under app_process, attach to the Shizuku binder directly, and
 * serve commands long-term over either:
 *
 *   1. A Unix-domain socket  (primary; --uds <path>)
 *      Default: /data/data/com.termux/files/usr/tmp/miuiser-shizuku.sock
 *   2. stdin/stdout REPL     (fallback; --stdio)
 *      Framed with ___MIUISEREND___ markers (matching ShizukuHelper.java convention)
 *
 * Binder attach strategy (first-success-wins):
 *   A. rikka.shizuku.ShizukuProvider#requestBinderForNonProviderProcess
 *      (reflective; only present if shizuku-provider.jar is on the classpath)
 *   B. ContentResolver.call("moe.shizuku.manager.shizuku", "requestBinder", ...)
 *      against the Shizuku Manager content provider
 *   C. Passive sticky listener (addBinderReceivedListenerSticky) + poll
 *
 * Caller package id is taken from $RISH_APPLICATION_ID (default com.termux).
 *
 * Process creation uses Shizuku.requireService() (reflective) -> IShizukuService
 * AIDL -> newProcess(), matching how rish itself works at runtime.
 *
 * Build:
 *   See ../../../../Build_Dex.sh
 *
 * Smoke-test on device:
 *   /system/bin/app_process \
 *       -Djava.class.path=$DEX $HOME \
 *       --nice-name=miuiser_shz_helper \
 *       ShizukuDirectLoader --uds /data/data/com.termux/files/usr/tmp/miuiser-shizuku.sock
 */

import android.content.Context;
import android.os.Bundle;
import android.os.IBinder;
import java.io.FileDescriptor;
import android.os.Looper;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import rikka.shizuku.Shizuku;
import rikka.shizuku.Shizuku.OnBinderReceivedListener;
import rikka.shizuku.Shizuku.OnRequestPermissionResultListener;

public class ShizukuDirectLoader {

    /* ------- constants ------- */
    private static final String END_MARKER = "___MIUISEREND___";
    private static final String DEFAULT_PKG = "com.termux";
    private static final String DEFAULT_UDS =
            "/data/data/com.termux/files/usr/tmp/miuiser-shizuku.sock";
    private static final int PERMISSION_REQUEST_CODE = 4711;
    private static final long BINDER_WAIT_MS   = 10_000;
    private static final long PERMISSION_WAIT_MS = 30_000;

    /* ------- shared state ------- */
    private static final CountDownLatch sBinderLatch = new CountDownLatch(1);
    private static final AtomicBoolean sPermissionGranted = new AtomicBoolean(false);
    private static String sPackageName;

    /* ===================================================================== */
    /* entry point                                                            */
    /* ===================================================================== */
    public static void main(String[] args) {
        boolean stdio  = false;
        String udsPath = DEFAULT_UDS;

        for (int i = 0; i < args.length; i++) {
            String a = args[i];
            if ("--stdio".equals(a)) {
                stdio = true;
            } else if ("--uds".equals(a) && i + 1 < args.length) {
                udsPath = args[++i];
            } else if ("-h".equals(a) || "--help".equals(a)) {
                printHelp();
                return;
            } else {
                err("unknown arg: " + a);
            }
        }

        sPackageName = envOrDefault("RISH_APPLICATION_ID", DEFAULT_PKG);
        err("ShizukuDirectLoader starting; pkg=" + sPackageName
                + " transport=" + (stdio ? "stdio" : ("uds:" + udsPath)));

        // Main looper is required for Shizuku IPC callbacks under app_process.
        if (Looper.getMainLooper() == null) {
            Looper.prepareMainLooper();
        }

        if (!acquireBinder()) {
            err("FATAL: could not attach to Shizuku binder. Is the Shizuku service running?");
            System.exit(2);
        }
        if (!ensurePermission()) {
            err("FATAL: Shizuku permission not granted for " + sPackageName);
            System.exit(3);
        }
        err("attached: uid=" + safeUid() + " apiVersion=" + safeApiVersion());

        try {
            if (stdio) {
                serveStdio();
            } else {
                serveUds(udsPath);
            }
        } catch (Exception e) {
            err("serve loop crashed: " + e);
            e.printStackTrace();
            System.exit(4);
        }
    }

    /* ===================================================================== */
    /* binder attach                                                          */
    /* ===================================================================== */

    private static boolean acquireBinder() {
        // Register the sticky listener first so we don't race.
        Shizuku.addBinderReceivedListenerSticky(new OnBinderReceivedListener() {
            public void onBinderReceived() {
                sBinderLatch.countDown();
            }
        });

        if (Shizuku.pingBinder()) {
            sBinderLatch.countDown(); // already up
            return true;
        }

        // Strategy A: ShizukuProvider.requestBinderForNonProviderProcess (needs provider jar)
        if (tryProviderAttach()) {
            err("attach: strategy A (ShizukuProvider) initiated");
            return waitForBinder();
        }

        // Strategy B: ContentResolver.call to Shizuku Manager
        if (tryContentProviderAttach()) {
            err("attach: strategy B (ContentResolver.call) initiated");
            return waitForBinder();
        }

        // Strategy C: just wait for the sticky listener to fire
        err("attach: strategies A/B unavailable -- waiting passively");
        return waitForBinder();
    }

    private static boolean waitForBinder() {
        try {
            return sBinderLatch.await(BINDER_WAIT_MS, TimeUnit.MILLISECONDS)
                    && Shizuku.pingBinder();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return false;
        }
    }

    /**
     * Reflectively call ShizukuProvider.requestBinderForNonProviderProcess so
     * we compile without a hard dep on shizuku-provider (it may not be on the
     * classpath on older setups).
     */
    private static boolean tryProviderAttach() {
        try {
            Class<?> sp = Class.forName("rikka.shizuku.ShizukuProvider");
            Context ctx = buildSystemContext();
            if (ctx == null) return false;
            for (Method m : sp.getMethods()) {
                if (!"requestBinderForNonProviderProcess".equals(m.getName())) continue;
                Class<?>[] p = m.getParameterTypes();
                try {
                    if (p.length >= 2
                            && Context.class.isAssignableFrom(p[0])
                            && p[1] == String.class) {
                        if (p.length == 2) {
                            m.invoke(null, ctx, sPackageName);
                        } else {
                            m.invoke(null, ctx, sPackageName, null);
                        }
                        return true;
                    }
                } catch (Throwable t) {
                    err("providerAttach invoke(" + p.length + " args): " + t);
                }
            }
        } catch (ClassNotFoundException e) {
            /* shizuku-provider not on classpath -- that's fine */
        } catch (Throwable t) {
            err("tryProviderAttach: " + t);
        }
        return false;
    }

    /**
     * Call the Shizuku Manager content provider directly:
     *   ContentResolver.call(uri, "requestBinder", null, bundle{package=...})
     * Then push the returned IBinder into Shizuku's static state via
     * Shizuku.onBinderReceived(IBinder, String) (reflective).
     */
    private static boolean tryContentProviderAttach() {
        try {
            Context ctx = buildSystemContext();
            if (ctx == null) return false;

            android.net.Uri uri = android.net.Uri.parse("content://moe.shizuku.manager.shizuku");
            Bundle reqBundle = new Bundle();
            reqBundle.putString("package", sPackageName);

            Bundle reply = null;
            try {
                reply = ctx.getContentResolver().call(uri, "requestBinder", null, reqBundle);
            } catch (Throwable t1) {
                try {
                    reply = ctx.getContentResolver().call(uri, "sendBinder", null, reqBundle);
                } catch (Throwable t2) {
                    err("CR.call failed: " + t1 + " / " + t2);
                }
            }
            if (reply == null) return false;
            reply.setClassLoader(ShizukuDirectLoader.class.getClassLoader());

            // Extract the IBinder -- stored as a binder extra or inside a
            // BinderContainer Parcelable depending on Shizuku version.
            IBinder binder = reply.getBinder("binder");
            if (binder == null) {
                android.os.Parcelable p = reply.getParcelable("binder");
                if (p != null) {
                    try {
                        Method gb = p.getClass().getDeclaredMethod("getBinder");
                        gb.setAccessible(true);
                        binder = (IBinder) gb.invoke(p);
                    } catch (Throwable ignored) { /* */ }
                }
            }
            if (binder == null) return false;

            // Push into Shizuku static cache.
            Method onRecv;
            try {
                onRecv = Shizuku.class.getDeclaredMethod(
                        "onBinderReceived", IBinder.class, String.class);
            } catch (NoSuchMethodException e) {
                onRecv = Shizuku.class.getDeclaredMethod("onBinderReceived", IBinder.class);
            }
            onRecv.setAccessible(true);
            if (onRecv.getParameterTypes().length == 2) {
                onRecv.invoke(null, binder, sPackageName);
            } else {
                onRecv.invoke(null, binder);
            }
            return true;
        } catch (Throwable t) {
            err("tryContentProviderAttach: " + t);
            return false;
        }
    }

    /**
     * Build a lightweight Context inside app_process via ActivityThread.systemMain()
     * (the same hidden API every headless Android tool uses).
     */
    private static Context buildSystemContext() {
        try {
            Class<?> at = Class.forName("android.app.ActivityThread");
            Method systemMain = at.getMethod("systemMain");
            Object thread = systemMain.invoke(null);
            Method getSystemCtx = at.getMethod("getSystemContext");
            return (Context) getSystemCtx.invoke(thread);
        } catch (Throwable t) {
            err("buildSystemContext: " + t);
            return null;
        }
    }

    /* ===================================================================== */
    /* permission                                                             */
    /* ===================================================================== */

    private static boolean ensurePermission() {
        try {
            if (Shizuku.checkSelfPermission()
                    == android.content.pm.PackageManager.PERMISSION_GRANTED) {
                sPermissionGranted.set(true);
                return true;
            }
        } catch (Throwable t) {
            err("checkSelfPermission: " + t);
        }

        Shizuku.addRequestPermissionResultListener(
                new OnRequestPermissionResultListener() {
                    public void onRequestPermissionResult(int code, int result) {
                        if (code == PERMISSION_REQUEST_CODE) {
                            sPermissionGranted.set(
                                    result == android.content.pm.PackageManager.PERMISSION_GRANTED);
                        }
                    }
                });
        try {
            Shizuku.requestPermission(PERMISSION_REQUEST_CODE);
        } catch (Throwable t) {
            err("requestPermission: " + t);
            return false;
        }

        long deadline = System.currentTimeMillis() + PERMISSION_WAIT_MS;
        while (System.currentTimeMillis() < deadline) {
            if (sPermissionGranted.get()) return true;
            try { Thread.sleep(100); } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return false;
            }
        }
        return false;
    }

    /* ===================================================================== */
    /* transport: UDS                                                         */
    /* ===================================================================== */

    private static void serveUds(String path) throws Exception {
        File f = new File(path);
        File parent = f.getParentFile();
        if (parent != null) parent.mkdirs();
        if (f.exists()) f.delete();

        // Bind an AF_UNIX SOCK_STREAM socket on the filesystem path.
        // android.net.LocalServerSocket only supports RESERVED (abstract) ns
        // via its String constructor, so we bind via Os.* and hand the FD to it.
        FileDescriptor fd = bindFilesystemUnixSocket(path);
        android.net.LocalServerSocket server = new android.net.LocalServerSocket(fd);

        // chmod 660 so the C daemon process (same uid group) can connect
        Runtime.getRuntime().exec(new String[]{"chmod", "660", path}).waitFor();

        err("UDS ready: " + path);
        while (true) {
            final android.net.LocalSocket client = server.accept();
            Thread t = new Thread(new Runnable() {
                public void run() { handleConnection(client); }
            }, "shz-conn");
            t.setDaemon(true);
            t.start();
        }
    }

    /**
     * Bind AF_UNIX / SOCK_STREAM at a filesystem path via android.system.Os,
     * using reflection on UnixSocketAddress (API 29+) so we stay compatible
     * with lower API levels where it's exposed as a hidden API.
     */
    private static FileDescriptor bindFilesystemUnixSocket(String path) throws Exception {
        FileDescriptor fd = android.system.Os.socket(
                android.system.OsConstants.AF_UNIX,
                android.system.OsConstants.SOCK_STREAM,
                0);

        // Try android.system.UnixSocketAddress.createFileSystem(path) reflectively.
        Class<?> usaClass = null;
        try {
            usaClass = Class.forName("android.system.UnixSocketAddress");
        } catch (ClassNotFoundException e) {
            // Older AOSP hides it differently -- fall back to raw sockaddr via
            // reflection on Os.bind(FileDescriptor, SocketAddress).
        }

        java.net.SocketAddress sockAddr = null;
        if (usaClass != null) {
            Method createFs = usaClass.getMethod("createFileSystem", String.class);
            sockAddr = (java.net.SocketAddress) createFs.invoke(null, path);
        } else {
            // Fallback: use java.net.UnixDomainSocketAddress (Java 16+)
            try {
                Class<?> udsa = Class.forName("java.net.UnixDomainSocketAddress");
                Method of = udsa.getMethod("of", String.class);
                sockAddr = (java.net.SocketAddress) of.invoke(null, path);
            } catch (ClassNotFoundException e2) {
                throw new IOException("Cannot construct a Unix socket address -- "
                        + "neither android.system.UnixSocketAddress nor "
                        + "java.net.UnixDomainSocketAddress is available.");
            }
        }

        // Os.bind takes (FileDescriptor, SocketAddress) since API 26+.
        Method bind = android.system.Os.class.getMethod(
                "bind", FileDescriptor.class, java.net.SocketAddress.class);
        bind.invoke(null, fd, sockAddr);
        android.system.Os.listen(fd, 8);
        return fd;
    }

    private static void handleConnection(android.net.LocalSocket client) {
        try {
            OutputStream rawOut = client.getOutputStream();
            BufferedReader in = new BufferedReader(
                    new InputStreamReader(client.getInputStream(), StandardCharsets.UTF_8));

            String line;
            while ((line = in.readLine()) != null) {
                String cmd = line.trim();
                if (cmd.isEmpty()) continue;
                if ("exit".equalsIgnoreCase(cmd)) break;
                String result = runShellCommand(cmd);
                // Length-prefixed framing: "<decimal_bytes>\n<payload>"
                byte[] payload = result.getBytes(StandardCharsets.UTF_8);
                String header  = payload.length + "\n";
                rawOut.write(header.getBytes(StandardCharsets.UTF_8));
                rawOut.write(payload);
                rawOut.flush();
            }
        } catch (IOException e) {
            err("connection error: " + e);
        } finally {
            try { client.close(); } catch (IOException ignored) { /* */ }
        }
    }

    /* ===================================================================== */
    /* transport: stdio REPL (--stdio)                                        */
    /* ===================================================================== */

    private static void serveStdio() throws IOException {
        BufferedReader in = new BufferedReader(
                new InputStreamReader(System.in, StandardCharsets.UTF_8));
        PrintWriter out = new PrintWriter(
                new OutputStreamWriter(System.out, StandardCharsets.UTF_8), true);

        String line;
        while ((line = in.readLine()) != null) {
            String cmd = line.trim();
            if (cmd.isEmpty()) continue;
            if ("exit".equalsIgnoreCase(cmd)) break;
            String result = runShellCommand(cmd);
            out.println(result);
            out.println(END_MARKER);
            out.flush();
        }
    }

    /* ===================================================================== */
    /* command execution                                                      */
    /* ===================================================================== */

    /**
     * Execute a shell command via the Shizuku IPC service.
     *
     * The public Shizuku API jar does not expose newProcess() directly -- it is
     * method on the AIDL interface (IShizukuService) returned by
     * Shizuku.requireService() (protected).  We call it reflectively so we don't
     * need a generated AIDL stub at compile time.
     */
    private static String runShellCommand(String cmd) {
        Process proc = null;
        try {
            // 1. Get the IShizukuService binder proxy
            Method requireService = Shizuku.class.getDeclaredMethod("requireService");
            requireService.setAccessible(true);
            Object service = requireService.invoke(null);
            if (service == null) return "ERROR: Shizuku service is null";

            // 2. Call IShizukuService.newProcess(String[] cmd, String[] env, String dir)
            //    The return type is IRemoteProcess (a Parcelable binder).
            Method newProcess = null;
            for (Method m : service.getClass().getMethods()) {
                if ("newProcess".equals(m.getName())) {
                    newProcess = m;
                    break;
                }
            }
            if (newProcess == null) return "ERROR: IShizukuService.newProcess not found";

            Object remoteProc = newProcess.invoke(service,
                    new String[]{"sh", "-c", cmd},
                    (String[]) null,
                    (String) null);
            if (remoteProc == null) return "ERROR: newProcess returned null";

            // 3. Wrap in ShizukuRemoteProcess to get standard java.lang.Process streams
            Class<?> srpClass = Class.forName("rikka.shizuku.ShizukuRemoteProcess");
            Class<?> irpClass = Class.forName("rikka.shizuku.server.IRemoteProcess");
            // Package-private ctor: ShizukuRemoteProcess(IRemoteProcess)
            java.lang.reflect.Constructor<?> ctor = srpClass.getDeclaredConstructor(irpClass);
            ctor.setAccessible(true);
            proc = (Process) ctor.newInstance(remoteProc);

            StringBuilder buf = new StringBuilder();
            try (BufferedReader r = new BufferedReader(
                         new InputStreamReader(proc.getInputStream()));
                 BufferedReader er = new BufferedReader(
                         new InputStreamReader(proc.getErrorStream()))) {
                String l;
                while ((l = r.readLine())  != null) buf.append(l).append('\n');
                while ((l = er.readLine()) != null) buf.append("[stderr] ").append(l).append('\n');
            }
            int rc = proc.waitFor();
            if (rc != 0) buf.append("[rc=").append(rc).append("]");
            return buf.toString();

        } catch (Throwable t) {
            return "ERROR: " + t.getClass().getSimpleName() + ": " + t.getMessage();
        } finally {
            if (proc != null) try { proc.destroy(); } catch (Throwable ignored) { /* */ }
        }
    }

    /* ===================================================================== */
    /* helpers                                                                */
    /* ===================================================================== */

    private static int safeUid() {
        try { return Shizuku.getUid(); }
        catch (Throwable t) { return -1; }
    }

    private static int safeApiVersion() {
        try { return Shizuku.getVersion(); }
        catch (Throwable t) { return -1; }
    }

    private static String envOrDefault(String key, String def) {
        String v = System.getenv(key);
        return (v == null || v.isEmpty()) ? def : v;
    }

    private static void err(String s) {
        System.err.println("[shz-direct] " + s);
    }

    private static void printHelp() {
        System.err.println(
            "ShizukuDirectLoader -- MiuiserPeruser third privileged route\n"
            + "usage: app_process -Djava.class.path=<dex> <cwd> "
            + "ShizukuDirectLoader [opts]\n"
            + "  --uds <path>   serve length-prefixed protocol over a UNIX-domain socket\n"
            + "  --stdio        serve a stdin/stdout REPL with " + END_MARKER + " framing\n"
            + "  -h, --help     this message\n"
            + "env:\n"
            + "  RISH_APPLICATION_ID   caller package (default " + DEFAULT_PKG + ")\n");
    }
}

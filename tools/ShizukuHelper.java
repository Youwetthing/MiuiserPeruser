import android.os.IBinder;
import android.system.ErrnoException;
import android.system.Os;
import java.io.BufferedReader;
import java.io.FileDescriptor;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import rikka.shizuku.Shizuku;

public class ShizukuHelper {
    private static final int BINDER_TIMEOUT_SEC = 15;
    private static CountDownLatch binderLatch = new CountDownLatch(1);

    public static void main(String[] args) {
        if (args.length == 0) {
            System.err.println("Usage: java -jar ShizukuHelper.jar <command>");
            System.exit(1);
        }

        // Wait for Shizuku binder
        Shizuku.addBinderReceivedListener(() -> binderLatch.countDown());
        if (Shizuku.getBinder() != null) binderLatch.countDown();

        try {
            if (!binderLatch.await(BINDER_TIMEOUT_SEC, TimeUnit.SECONDS)) {
                System.err.println("ERROR: Shizuku binder timeout");
                System.exit(1);
            }
        } catch (InterruptedException e) {
            System.err.println("ERROR: Interrupted");
            System.exit(1);
        }

        // Ensure permission (should be pre‑granted)
        if (Shizuku.checkSelfPermission() != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            Shizuku.requestPermission(1001);
            try { Thread.sleep(500); } catch (InterruptedException ignored) {}
            if (Shizuku.checkSelfPermission() != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                System.err.println("ERROR: Permission denied");
                System.exit(1);
            }
        }

        String command = String.join(" ", args);
        try {
            Process process = Shizuku.createProcess(
                new String[]{"sh", "-c", command},
                null, null,
                FileDescriptor.in, FileDescriptor.out, FileDescriptor.err
            );

            // Copy output to stdout
            InputStream is = process.getInputStream();
            byte[] buf = new byte[8192];
            int len;
            while ((len = is.read(buf)) != -1) {
                System.out.write(buf, 0, len);
            }
            int exitCode = process.waitFor();
            System.exit(exitCode);
        } catch (Exception e) {
            System.err.println("ERROR: " + e.getMessage());
            System.exit(1);
        }
    }
}

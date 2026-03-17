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
    private static final String END_MARKER = "___MIUISEREND___";
    private static final int PERMISSION_REQUEST_CODE = 1001;
    private static CountDownLatch binderLatch = new CountDownLatch(1);
    private static volatile boolean permissionGranted = false;

    public static void main(String[] args) {
        System.err.println("ShizukuHelper starting...");

        // Wait for Shizuku binder
        Shizuku.addBinderReceivedListener(() -> binderLatch.countDown());
        if (Shizuku.getBinder() != null) binderLatch.countDown();

        try {
            if (!binderLatch.await(10, TimeUnit.SECONDS)) {
                System.err.println("Shizuku binder not available");
                System.exit(1);
            }
        } catch (InterruptedException e) {
            System.err.println("Interrupted while waiting for binder");
            System.exit(1);
        }

        // Permission handling
        if (Shizuku.checkSelfPermission() != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            System.err.println("Requesting permission...");
            Shizuku.addRequestPermissionResultListener((requestCode, grantResult) -> {
                if (requestCode == PERMISSION_REQUEST_CODE) {
                    permissionGranted = (grantResult == android.content.pm.PackageManager.PERMISSION_GRANTED);
                    if (!permissionGranted) {
                        System.err.println("Permission denied");
                        System.exit(1);
                    }
                }
            });
            Shizuku.requestPermission(PERMISSION_REQUEST_CODE);
            // Wait a bit for user to grant (simplified)
            for (int i = 0; i < 30; i++) {
                if (permissionGranted) break;
                try { Thread.sleep(100); } catch (InterruptedException e) {}
            }
            if (!permissionGranted) {
                System.err.println("Permission not granted");
                System.exit(1);
            }
        }

        System.err.println("Shizuku UID: " + Shizuku.getUid());

        // Main command loop – read from stdin, execute, write to stdout
        try (BufferedReader in = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
             PrintWriter out = new PrintWriter(new OutputStreamWriter(System.out, StandardCharsets.UTF_8), true)) {

            String line;
            while ((line = in.readLine()) != null) {
                if (line.trim().equalsIgnoreCase("exit")) break;
                String result = executeCommand(line);
                out.println(result);
                out.println(END_MARKER);
                out.flush();
            }
        } catch (Exception e) {
            e.printStackTrace();
            System.exit(1);
        }
    }

    private static String executeCommand(String cmd) {
        try {
            Process process = Shizuku.createProcess(
                new String[]{"sh", "-c", cmd},
                null,
                null,
                FileDescriptor.in,
                FileDescriptor.out,
                FileDescriptor.err
            );

            StringBuilder output = new StringBuilder();
            try (InputStream is = process.getInputStream();
                 BufferedReader reader = new BufferedReader(new InputStreamReader(is))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    output.append(line).append("\n");
                }
            }

            int exitCode = process.waitFor();
            if (exitCode != 0) {
                return "ERROR: exit code " + exitCode + "\n" + output.toString().trim();
            }
            return output.toString().trim();
        } catch (Exception e) {
            return "ERROR: " + e.getMessage();
        }
    }
}

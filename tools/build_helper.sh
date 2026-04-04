#!/data/data/com.termux/files/usr/bin/bash
set -e
echo "Compiling ShizukuHelper.java..."
cd java-helper
javac -cp libs/shizuku-api.jar -d . src/main/java/ShizukuHelper.java
if [ $? -eq 0 ]; then
    echo "Compilation successful."
    jar cf shizuku-helper.jar *.class
    echo "JAR created: java-helper/shizuku-helper.jar"
else
    echo "Compilation failed."
    exit 1
fi

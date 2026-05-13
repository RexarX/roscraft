package net.roscraft.bridge;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/**
 * Loads the native library for the bridge JNI layer.
 */
final class NativeLoader {

  private NativeLoader() {}

  private static final String LIB_NAME = "roscraft_bridge_jni";
  private static volatile boolean loaded = false;

  static synchronized void ensureLoaded() {
    if (loaded) return;
    loadFromJar();
    loaded = true;
  }

  private static void loadFromJar() {
    String resourcePath = "natives/" + osClassifier() + "/" + systemLibName();
    try (InputStream in = NativeLoader.class.getClassLoader().getResourceAsStream(resourcePath)) {
      if (in != null) {
        loadFromStream(in);
        return;
      }
    } catch (IOException e) {
      throw new UnsatisfiedLinkError(
          "Failed to extract native library " + resourcePath + ": " + e.getMessage());
    }

    System.loadLibrary(LIB_NAME);
  }

  private static void loadFromStream(InputStream in) throws IOException {
    Path tempDir = Files.createTempDirectory("roscraft-natives");
    tempDir.toFile().deleteOnExit();
    Path tempLib = tempDir.resolve(systemLibName());
    Files.copy(in, tempLib, StandardCopyOption.REPLACE_EXISTING);
    tempLib.toFile().deleteOnExit();
    System.load(tempLib.toAbsolutePath().toString());
  }

  static String osClassifier() {
    String os = System.getProperty("os.name").toLowerCase();
    String osLabel;
    if (os.contains("win")) {
      osLabel = "windows";
    } else if (os.contains("mac")) {
      osLabel = "macos";
    } else {
      osLabel = "linux";
    }
    String arch = System.getProperty("os.arch").toLowerCase();
    String archLabel = arch.contains("aarch64") || arch.contains("arm64") ? "arm64" : "x86_64";
    return osLabel + "-" + archLabel;
  }

  static String systemLibName() {
    String os = System.getProperty("os.name").toLowerCase();
    if (os.contains("win")) return LIB_NAME + ".dll";
    if (os.contains("mac")) return "lib" + LIB_NAME + ".dylib";
    return "lib" + LIB_NAME + ".so";
  }
}

package net.roscraft.bridge;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;

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
    String resourceDir = "natives/" + osClassifier() + "/";
    List<String> libNames = listNativeLibNames(resourceDir);
    if (!libNames.contains(systemLibName())) {
      System.loadLibrary(LIB_NAME);
      return;
    }

    try {
      loadNativeLibsFromJar(resourceDir, libNames);
    } catch (IOException e) {
      throw new UnsatisfiedLinkError(
          "Failed to extract native libraries from " + resourceDir + ": " + e.getMessage());
    }
  }

  private static List<String> listNativeLibNames(String resourceDir) {
    List<String> names = new ArrayList<>();
    try (InputStream in =
        NativeLoader.class.getClassLoader().getResourceAsStream(resourceDir + "natives.list")) {
      if (in == null) {
        return names;
      }
      byte[] buf = in.readAllBytes();
      for (String line : new String(buf).split("\n")) {
        String trimmed = line.trim();
        if (!trimmed.isEmpty()) {
          names.add(trimmed);
        }
      }
    } catch (IOException e) {
      return names;
    }
    return names;
  }

  private static void loadNativeLibsFromJar(String resourceDir, List<String> libNames)
      throws IOException {
    Path tempDir = Files.createTempDirectory("roscraft-natives");
    tempDir.toFile().deleteOnExit();

    for (String libName : libNames) {
      String resourcePath = resourceDir + libName;
      try (InputStream in = NativeLoader.class.getClassLoader().getResourceAsStream(resourcePath)) {
        if (in == null) continue;
        Path target = tempDir.resolve(libName);
        Files.copy(in, target, StandardCopyOption.REPLACE_EXISTING);
        target.toFile().deleteOnExit();
      }
    }

    List<String> remaining = new ArrayList<>(libNames);
    int prevSize = remaining.size() + 1;
    while (!remaining.isEmpty() && remaining.size() < prevSize) {
      prevSize = remaining.size();
      List<String> stillRemaining = new ArrayList<>();
      for (String libName : remaining) {
        try {
          System.load(tempDir.resolve(libName).toAbsolutePath().toString());
        } catch (UnsatisfiedLinkError e) {
          stillRemaining.add(libName);
        }
      }
      remaining = stillRemaining;
    }

    String mainLib = systemLibName();
    if (!remaining.isEmpty() && remaining.contains(mainLib)) {
      throw new UnsatisfiedLinkError(
          "Failed to load " + mainLib + ". Remaining deps: " + String.join(", ", remaining));
    }
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

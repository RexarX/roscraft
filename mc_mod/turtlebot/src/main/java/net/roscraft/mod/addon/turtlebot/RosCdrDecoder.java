package net.roscraft.mod.addon.turtlebot;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.Optional;

/** Minimal ROS 2 CDR decoders for turtlebot message types. */
final class RosCdrDecoder {

  private static final double EPSILON = 1.0e-6;

  private RosCdrDecoder() {}

  static Optional<Boolean> decodeBool(byte[] payload) {
    if (payload == null || payload.length == 0) {
      return Optional.empty();
    }

    int offset = encapsulationOffset(payload);
    if (offset < payload.length) {
      return Optional.of(payload[offset] != 0);
    }

    return decodeBoolFromText(payload);
  }

  static Optional<String> decodeString(byte[] payload) {
    if (payload == null || payload.length == 0) {
      return Optional.empty();
    }

    int offset = encapsulationOffset(payload);
    ByteBuffer buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN);
    if (offset + 4 <= payload.length) {
      buffer.position(offset);
      int length = buffer.getInt();
      if (length >= 0 && offset + 4 + length <= payload.length) {
        byte[] data = new byte[length];
        buffer.get(data);
        return Optional.of(new String(data, StandardCharsets.UTF_8));
      }
    }

    return decodeStringFromText(payload);
  }

  static Optional<Twist> decodeTwist(byte[] payload) {
    if (payload == null || payload.length == 0) {
      return Optional.empty();
    }

    int offset = encapsulationOffset(payload);
    ByteBuffer buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN);
    if (offset + 48 <= payload.length) {
      buffer.position(offset);
      double linearX = buffer.getDouble();
      double linearY = buffer.getDouble();
      double linearZ = buffer.getDouble();
      double angularX = buffer.getDouble();
      double angularY = buffer.getDouble();
      double angularZ = buffer.getDouble();
      return Optional.of(new Twist(linearX, linearY, linearZ, angularX, angularY, angularZ));
    }

    return decodeTwistFromText(payload);
  }

  static boolean isStop(Twist twist) {
    return Math.abs(twist.linearX()) < EPSILON
        && Math.abs(twist.linearY()) < EPSILON
        && Math.abs(twist.linearZ()) < EPSILON
        && Math.abs(twist.angularX()) < EPSILON
        && Math.abs(twist.angularY()) < EPSILON
        && Math.abs(twist.angularZ()) < EPSILON;
  }

  private static int encapsulationOffset(byte[] payload) {
    if (payload.length >= 4 && payload[0] == 0x00 && payload[1] == 0x01) {
      return 4;
    }
    return 0;
  }

  private static Optional<Boolean> decodeBoolFromText(byte[] payload) {
    String text = decodePayloadText(payload).toLowerCase(Locale.ROOT);
    if (text.isBlank()) {
      return Optional.empty();
    }
    if (text.equals("true")
        || text.equals("1")
        || text.equals("yes")
        || text.equals("on")
        || text.equals("spawned")) {
      return Optional.of(true);
    }
    if (text.equals("false")
        || text.equals("0")
        || text.equals("no")
        || text.equals("off")
        || text.equals("despawned")) {
      return Optional.of(false);
    }
    return Optional.empty();
  }

  private static Optional<String> decodeStringFromText(byte[] payload) {
    String text = decodePayloadText(payload);
    return text.isBlank() ? Optional.empty() : Optional.of(text);
  }

  private static Optional<Twist> decodeTwistFromText(byte[] payload) {
    String text = decodePayloadText(payload);
    if (text.isBlank()) {
      return Optional.empty();
    }

    double linearX = readField(text, "linear_x", "linear.x");
    double linearZ = readField(text, "linear_z", "linear.z");
    double angularZ = readField(text, "angular_z", "angular.z");
    if (Double.isNaN(linearX) && Double.isNaN(linearZ) && Double.isNaN(angularZ)) {
      return Optional.empty();
    }

    return Optional.of(
        new Twist(nanToZero(linearX), 0.0, nanToZero(linearZ), 0.0, 0.0, nanToZero(angularZ)));
  }

  private static double readField(String text, String... keys) {
    for (String key : keys) {
      int index = text.indexOf(key);
      if (index < 0) {
        continue;
      }
      int start = index + key.length();
      while (start < text.length()
          && !Character.isDigit(text.charAt(start))
          && text.charAt(start) != '-') {
        start++;
      }
      int end = start;
      while (end < text.length()
          && (Character.isDigit(text.charAt(end))
              || text.charAt(end) == '.'
              || text.charAt(end) == '-'
              || text.charAt(end) == 'e'
              || text.charAt(end) == 'E'
              || text.charAt(end) == '+')) {
        end++;
      }
      if (end > start) {
        try {
          return Double.parseDouble(text.substring(start, end));
        } catch (NumberFormatException ignored) {
          return Double.NaN;
        }
      }
    }
    return Double.NaN;
  }

  private static double nanToZero(double value) {
    return Double.isNaN(value) ? 0.0 : value;
  }

  static String decodePayloadText(byte[] payload) {
    String text = new String(payload, StandardCharsets.UTF_8).trim();
    int dataIndex = text.indexOf("data:");
    if (dataIndex >= 0) {
      text = text.substring(dataIndex + 5).trim();
    }
    if ((text.startsWith("'") && text.endsWith("'"))
        || (text.startsWith("\"") && text.endsWith("\""))) {
      text = text.substring(1, text.length() - 1);
    }
    return text.trim();
  }

  record Twist(
      double linearX,
      double linearY,
      double linearZ,
      double angularX,
      double angularY,
      double angularZ) {}
}

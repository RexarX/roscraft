package net.roscraft.bridge;

import java.nio.ByteBuffer;
import roscraft.bridge.fbs.BridgePacket;

public final class JniBridge extends AbstractPacketBridge {

  static {
    NativeLoader.ensureLoaded();
  }

  private final class NativeCallback {
    void onPacket(byte[] packetBytes) {
      if (packetBytes == null || packetBytes.length == 0) {
        return;
      }

      var buffer = ByteBuffer.wrap(packetBytes);
      if (!BridgePacket.BridgePacketBufferHasIdentifier(buffer)) {
        return;
      }

      packetDispatcher.dispatch(BridgePacket.getRootAsBridgePacket(buffer));
    }
  }

  private JniBridge() {}

  public static JniBridge create() {
    if (!nativeCreate()) {
      throw new IllegalStateException("Failed to initialise native ROS2 JNI bridge. "
          + "Check that ROS2 is installed and sourced correctly.");
    }
    var bridge = new JniBridge();
    nativeRegisterCallback(bridge.new NativeCallback());
    return bridge;
  }

  @Override
  public void tick() {
    checkOpen("JniBridge");
    nativeTick();
  }

  @Override
  protected void sendPacket(ByteBuffer buf) {
    ByteBuffer buffer = buf.duplicate();
    byte[] bytes = new byte[buffer.remaining()];
    buffer.get(bytes);
    nativeSendPacket(bytes);
  }

  @Override
  public void close() {
    if (closed.compareAndSet(false, true)) {
      nativeDestroy();
    }
  }

  private static native boolean nativeCreate();

  private static native void nativeDestroy();

  private static native void nativeRegisterCallback(Object callback);

  private static native void nativeTick();

  private static native void nativeSendPacket(byte[] packet);
}

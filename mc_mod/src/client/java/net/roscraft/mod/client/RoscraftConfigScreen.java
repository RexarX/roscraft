package net.roscraft.mod.client;

import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.gui.screen.Screen;
import net.minecraft.client.gui.widget.ButtonWidget;
import net.minecraft.client.gui.widget.TextFieldWidget;
import net.minecraft.text.Text;
import net.roscraft.mod.RoscraftConfig;
import net.roscraft.mod.bridge.BridgeFactory;

public final class RoscraftConfigScreen extends Screen {

    private final Screen parent;
    private final boolean jniAvailable;

    private String bridgeType;
    private final String initialHost;
    private final int initialPort;

    private ButtonWidget modeButton;
    private TextFieldWidget hostField;
    private TextFieldWidget portField;

    private Text validationError;

    public RoscraftConfigScreen(Screen parent) {
        super(Text.translatable("text.roscraft.config.title"));
        this.parent = parent;

        var config = RoscraftConfig.load();
        this.jniAvailable = BridgeFactory.isJniAvailable();
        this.bridgeType = config.bridgeType();
        this.initialHost = config.networkHost();
        this.initialPort = config.networkPort();

        if ("jni".equals(this.bridgeType) && !this.jniAvailable) {
            this.bridgeType = "network";
        }
    }

    @Override
    protected void init() {
        int fieldWidth = 220;
        int left = this.width / 2 - fieldWidth / 2;
        int top = this.height / 4;

        modeButton =
                this.addDrawableChild(
                        ButtonWidget.builder(modeButtonText(), button -> onModeButtonPressed())
                                .dimensions(left, top + 12, fieldWidth, 20)
                                .build());

        hostField =
                this.addDrawableChild(
                        new TextFieldWidget(
                                this.textRenderer,
                                left,
                                top + 56,
                                fieldWidth,
                                20,
                                Text.translatable("text.roscraft.config.host")));
        hostField.setText(initialHost);

        portField =
                this.addDrawableChild(
                        new TextFieldWidget(
                                this.textRenderer,
                                left,
                                top + 96,
                                fieldWidth,
                                20,
                                Text.translatable("text.roscraft.config.port")));
        portField.setText(String.valueOf(initialPort));
        portField.setTextPredicate(value -> value.isEmpty() || value.matches("\\d{1,5}"));

        this.addDrawableChild(
                ButtonWidget.builder(
                                Text.translatable("text.roscraft.config.save"), button -> save())
                        .dimensions(this.width / 2 - 104, top + 144, 100, 20)
                        .build());

        this.addDrawableChild(
                ButtonWidget.builder(
                                Text.translatable("text.roscraft.config.cancel"), button -> close())
                        .dimensions(this.width / 2 + 4, top + 144, 100, 20)
                        .build());

        updateFieldState();
        setInitialFocus(hostField);
    }

    @Override
    public void render(DrawContext context, int mouseX, int mouseY, float delta) {
        this.renderBackground(context, mouseX, mouseY, delta);
        super.render(context, mouseX, mouseY, delta);

        int left = this.width / 2 - 110;
        int top = this.height / 4;

        context.drawCenteredTextWithShadow(
                this.textRenderer, this.title, this.width / 2, top - 14, 0xFFFFFF);

        context.drawTextWithShadow(
                this.textRenderer,
                Text.translatable("text.roscraft.config.mode"),
                left,
                top,
                0xA0A0A0);

        context.drawTextWithShadow(
                this.textRenderer,
                Text.translatable("text.roscraft.config.host"),
                left,
                top + 44,
                0xA0A0A0);

        context.drawTextWithShadow(
                this.textRenderer,
                Text.translatable("text.roscraft.config.port"),
                left,
                top + 84,
                0xA0A0A0);

        if (!jniAvailable) {
            context.drawCenteredTextWithShadow(
                    this.textRenderer,
                    Text.translatable("text.roscraft.config.jni_unavailable"),
                    this.width / 2,
                    top + 124,
                    0xFFAA00);
        } else if ("jni".equals(bridgeType)) {
            context.drawCenteredTextWithShadow(
                    this.textRenderer,
                    Text.translatable("text.roscraft.config.jni_mode_note"),
                    this.width / 2,
                    top + 124,
                    0xA0A0A0);
        }

        if (validationError != null) {
            context.drawCenteredTextWithShadow(
                    this.textRenderer, validationError, this.width / 2, top + 172, 0xFF5555);
        }
    }

    @Override
    public void close() {
        if (this.client != null) {
            this.client.setScreen(parent);
        }
    }

    private void onModeButtonPressed() {
        if (jniAvailable) {
            bridgeType = "network".equals(bridgeType) ? "jni" : "network";
        } else {
            bridgeType = "network";
        }
        modeButton.setMessage(modeButtonText());
        updateFieldState();
    }

    private void updateFieldState() {
        boolean networkMode = "network".equals(bridgeType);
        hostField.setEditable(networkMode);
        hostField.active = networkMode;
        portField.setEditable(networkMode);
        portField.active = networkMode;
    }

    private Text modeButtonText() {
        Text modeText =
                "network".equals(bridgeType)
                        ? Text.translatable("text.roscraft.config.mode.network")
                        : Text.translatable("text.roscraft.config.mode.jni");
        return Text.translatable("text.roscraft.config.mode.value", modeText);
    }

    private void save() {
        validationError = null;

        int port;
        try {
            port = Integer.parseInt(portField.getText().trim());
        } catch (NumberFormatException e) {
            validationError = Text.translatable("text.roscraft.config.error.invalid_port");
            return;
        }

        String host = hostField.getText().trim();
        try {
            RoscraftConfig config = new RoscraftConfig(bridgeType, host, port);
            config.save();
            close();
        } catch (IllegalArgumentException e) {
            validationError = Text.literal(e.getMessage());
        }
    }
}

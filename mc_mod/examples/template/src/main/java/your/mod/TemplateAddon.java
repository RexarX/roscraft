package your.mod;

import com.mojang.brigadier.arguments.IntegerArgumentType;
import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import java.nio.charset.StandardCharsets;
import java.util.List;
import net.minecraft.server.command.CommandManager;
import net.minecraft.server.command.ServerCommandSource;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.BridgeOperations;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.addon.AddonContext;
import net.roscraft.mod.addon.RoscraftAddon;

/**
 * Minimal roscraft addon template.  Copy this directory into your own project,
 * rename the package, pick a unique {@link #addonId()}, and add to
 * {@code fabric.mod.json}.
 *
 * <pre>{@code
 * // fabric.mod.json entrypoints:
 * "roscraft:addon": ["your.mod.TemplateAddon"]
 * }</pre>
 */
public class TemplateAddon implements RoscraftAddon {

    private AddonContext ctx;

    @Override
    public String addonId() {
        return "template";
    }

    @Override
    public void init(AddonContext ctx) {
        this.ctx = ctx;
    }

    // ── Commands (appear under /ros) ──────────────────────────────────

    @Override
    public List<LiteralArgumentBuilder<ServerCommandSource>> commands() {
        return List.of(
            CommandManager.literal("template")
                .executes(c -> help(c.getSource()))
                .then(
                    CommandManager.literal("pub").then(
                        CommandManager.argument(
                            "count",
                            IntegerArgumentType.integer(1, 100)
                        ).executes(c ->
                            publishN(
                                c.getSource(),
                                IntegerArgumentType.getInteger(c, "count")
                            )
                        )
                    )
                )
        );
    }

    // ── Bridge event handler ──────────────────────────────────────────

    @Override
    public void onBridgeEvent(BridgeEvent event) {
        ctx
            .bridgeIfConnected()
            .ifPresent(bridge -> {
                switch (event) {
                    case BridgeEvent.TopicPayload p -> bridge.topics().publish(
                        "/roscraft/example/out",
                        "std_msgs/msg/String",
                        String.format(
                            "data: 'echo: topic=%s bytes=%d'",
                            p.topicName(),
                            p.payloadLength()
                        ).getBytes(StandardCharsets.UTF_8)
                    );
                    case BridgeEvent.BridgeError e -> bridge.topics().publish(
                        "/roscraft/example/out",
                        "std_msgs/msg/String",
                        String.format(
                            "data: 'error: code=%s msg=%s'",
                            e.errorCode(),
                            e.errorMessage()
                        ).getBytes(StandardCharsets.UTF_8)
                    );
                    default -> {
                    }
                }
            });
    }

    // ── Command implementations ───────────────────────────────────────

    private int help(ServerCommandSource src) {
        src.sendMessage(
            Text.literal("/ros template pub <count> — publish N messages")
        );
        return 1;
    }

    private int publishN(ServerCommandSource src, int count) {
        if (!ctx.isBridgeConnected()) {
            src.sendMessage(
                Text.literal("[Roscraft] ")
                    .formatted(Formatting.AQUA)
                    .append(
                        Text.literal(
                            "Bridge not connected. Run /ros connection connect first."
                        ).formatted(Formatting.RED)
                    )
            );
            return 0;
        }

        var bridge = ctx.bridgeIfConnected().get();
        for (int i = 0; i < count; i++) {
            bridge.topics().publish(
                "/roscraft/example/out",
                "std_msgs/msg/String",
                String.format("data: 'template #%d'", i).getBytes(
                    StandardCharsets.UTF_8
                )
            );
        }
        src.sendMessage(
            Text.literal("[Roscraft] ")
                .formatted(Formatting.AQUA)
                .append(
                    Text.literal("Published " + count + " messages.").formatted(
                        Formatting.GREEN
                    )
                )
        );
        return 1;
    }
}

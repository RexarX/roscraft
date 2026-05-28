package net.roscraft.mod.addon.minecraft;

import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import java.util.Collections;
import java.util.List;
import net.minecraft.server.command.ServerCommandSource;

/**
 * Optional extension for addons that register Brigadier commands under {@code /ros}.
 *
 * <p>Kept in the mod module so the core {@link net.roscraft.mod.addon.RoscraftAddon}
 * API does not require Minecraft types. Implement this interface alongside
 * {@code RoscraftAddon} in your addon mod.
 */
public interface RoscraftAddonCommands {

  default List<LiteralArgumentBuilder<ServerCommandSource>> commands() {
    return Collections.emptyList();
  }
}

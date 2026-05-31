from __future__ import annotations

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node, PushRosNamespace


def _parse_turtle_names(raw_names: str) -> list[str]:
    names = [name.strip().strip("/") for name in raw_names.split(",")]
    return [name for name in names if name]


def _is_truthy(raw_value: str) -> bool:
    return raw_value.strip().lower() in {"1", "true", "yes", "on"}


def _build_launch_entities(context, *_args, **_kwargs):
    turtle_names = _parse_turtle_names(
        LaunchConfiguration("turtle_names").perform(context)
    )
    if not turtle_names:
        turtle_names = ["turtle1"]

    launch_keyboard = _is_truthy(
        LaunchConfiguration("launch_keyboard").perform(context)
    )
    keyboard_namespace = LaunchConfiguration("keyboard_namespace").perform(context)
    keyboard_namespace = keyboard_namespace.strip().strip("/")

    actions: list[object] = []
    for turtle_name in turtle_names:
        actions.append(
            GroupAction(
                [
                    PushRosNamespace(turtle_name),
                    Node(
                        package="roscraft_turtlebot",
                        executable="roscraft_turtlebot_lifecycle",
                        output="screen",
                    ),
                    Node(
                        package="roscraft_turtlebot",
                        executable="roscraft_turtlebot_movement",
                        output="screen",
                    ),
                ]
            )
        )

    if launch_keyboard:
        control_namespace = keyboard_namespace or turtle_names[0]
        actions.append(
            GroupAction(
                [
                    PushRosNamespace(control_namespace),
                    Node(
                        package="roscraft_turtlebot",
                        executable="roscraft_turtlebot_keyboard",
                        output="screen",
                        emulate_tty=True,
                    ),
                ]
            )
        )

    return actions


def generate_launch_description() -> LaunchDescription:
    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "turtle_names",
                default_value="turtle1",
                description="Comma-separated turtle instance namespaces.",
            ),
            DeclareLaunchArgument(
                "launch_keyboard",
                default_value="false",
                description="Launch the keyboard input node.",
            ),
            DeclareLaunchArgument(
                "keyboard_namespace",
                default_value="",
                description=(
                    "Namespace for the keyboard input node. Defaults to the first "
                    "turtle namespace."
                ),
            ),
            OpaqueFunction(function=_build_launch_entities),
        ]
    )

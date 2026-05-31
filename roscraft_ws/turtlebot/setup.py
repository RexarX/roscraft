import os
from glob import glob

from setuptools import setup

package_name = "roscraft_turtlebot"

setup(
    name=package_name,
    version="0.1.0",
    packages=[package_name],
    data_files=[
        (
            "share/ament_index/resource_index/packages",
            ["resource/" + package_name],
        ),
        ("share/" + package_name, ["package.xml"]),
        (
            os.path.join("share", package_name, "launch"),
            glob(os.path.join("launch", "*.launch.py")),
        ),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="Denis Plishko",
    maintainer_email="who727cares@gmail.com",
    description="ROS 2 turtlebot control nodes for Roscraft.",
    license="MIT",
    entry_points={
        "console_scripts": [
            "roscraft_turtlebot_keyboard = roscraft_turtlebot.keyboard_input_node:main",
            "roscraft_turtlebot_lifecycle = roscraft_turtlebot.lifecycle_node:main",
            "roscraft_turtlebot_movement = roscraft_turtlebot.movement_node:main",
        ],
    },
)

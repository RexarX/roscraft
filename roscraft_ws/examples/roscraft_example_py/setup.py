from setuptools import setup

package_name = "roscraft_example_py"

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
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="Denis Plishko",
    maintainer_email="who727cares@gmail.com",
    description="Example ROS 2 Python node for Roscraft bridge.",
    license="MIT",
    entry_points={
        "console_scripts": [
            "roscraft_bridge_demo = roscraft_example_py.bridge_demo_node:main",
            "roscraft_addon_event_demo = roscraft_example_py.addon_event_demo:main",
        ],
    },
)

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    config_path = os.path.join(
        get_package_share_directory('nmsi_lab'), 'config', 'nav_config.yaml')

    return LaunchDescription([
        Node(
            package='nmsi_lab',
            executable='a_star.py',
            name='a_star',
            output='screen',
            parameters=[config_path]
        ),
        Node(
            package='nmsi_lab',
            executable='path_follower',
            name='path_follower',
            output='screen',
            parameters=[config_path]
        ),
    ])

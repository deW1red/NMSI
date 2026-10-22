#!/usr/bin/env python3

from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import OccupancyGrid, Path
import rclpy
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from rclpy.time import Time
from tf2_ros import Buffer, TransformException, TransformListener
from math import sqrt,floor

class AStarCalculator(Node):
    def __init__(self) -> None:
        super().__init__('a_star')

        self.fixed_frame = 'map'
        self.robot_frame = 'base_link'

        self.subscription = self.create_subscription(
            PoseStamped, '/goal_pose', self.pose_callback, 10)

        map_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL)
        self.dilated_map_subscription = self.create_subscription(
            OccupancyGrid, '/map_dilated', self.dilated_map_callback, map_qos)
        self.cost_map_subscription = self.create_subscription(
            OccupancyGrid, '/map_cost', self.cost_map_callback, map_qos)

        self.publisher = self.create_publisher(Path, '/path', 10)
        self.map_dilated: OccupancyGrid | None = None
        self.map_cost: OccupancyGrid | None = None

        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)

    def pose_callback(self, pose: PoseStamped) -> None:
        try:
            trans = self.tf_buffer.lookup_transform(
                self.fixed_frame, self.robot_frame, Time())
        except TransformException as exc:
            self.get_logger().warning(
                f'Could not transform {self.robot_frame} to '
                f'{self.fixed_frame}: {exc}')
            return

        start = PoseStamped()
        start.header.frame_id = 'map'
        start.header.stamp = self.get_clock().now().to_msg()
        start.pose.position.x = trans.transform.translation.x
        start.pose.position.y = trans.transform.translation.y
        start.pose.position.z = trans.transform.translation.z
        start.pose.orientation = trans.transform.rotation
        self.a_star(start, pose)

    def dilated_map_callback(self, map_msg: OccupancyGrid) -> None:
        self.map_dilated = map_msg

    def cost_map_callback(self, map_msg: OccupancyGrid) -> None:
        self.map_cost = map_msg

    def a_star(self, start: PoseStamped, end: PoseStamped) -> None:
        self.update_map_info()

        goal_position = {"x" = end.pose.position.x,"y" = end.pose.position.y}

        open = []
        closed = []

        open.append(build_tile(start.pose.position.x,start.pose.position.y,0+euclidean_distance(dgsfb),None))    

        while open:

            min_f = 1000000000
            currentTile = None
            for tile in open
                if(tile."f" < min_f):
                    min_f = tile."f"
                    currentTile = tile

            neighbors = []
            
            


        calculated_path = Path()
        calculated_path.header.frame_id = 'map'
        calculated_path.header.stamp = self.get_clock().now().to_msg()
        calculated_path.poses.append(start)
        calculated_path.poses.append(end)
        self.publisher.publish(calculated_path)

    def update_map_info(self):
        self.map_width = self.map_dilated.info.width
        self.map_height = self.map_dilated.info.height
        self.map_ox = self.map_dilated.info.origin.position.x
        self.map_oy = self.map_dilated.info.origin.position.y
        self.map_res = self.map_dilated.info.resolution
    
    def list_position(self,ix,iy):
        return self.map_width*iy + ix

    def meter_to_index(self,x,y):
        

        if(x < 0):
            x = 0
        if(y < 0):
            y = 0
        if(x > self.map_width):
            x = self.map_width - self.map_res
        if(y > self.map_height):
            y = self.map_height - self.map_res

        ix = floor((x - self.map_ox)/self.map_res)
        iy = floor((y - self.map_oy)/self.map_res)

        return ix, iy

    def index_to_meter(self,ix,iy):

        x = self.map_ox + (ix + 0.5)*self.map_res
        y = self.map_oy + (iy + 0.5)*self.map_res

        return x, y

    def get_tile_cost(self,ix,iy):
        return self.map_cost.data[list_position(self,ix,iy)]

    def is_tile_reachable(self,ix,iy):

        if(ix < 0 or iy < 0 or index_to_meter(ix) >= self.map_width or index_to_meter(iy) >= self.map_height):
            return False

        is_occupied = self.map_dilated.data[list_position(self,ix,iy)]

        if(is_occupied):
            return False
        else:
            return True


def euclidean_distance(x1,y1,x2,y2):
    return sqrt((x2-x1)**2+(y2-y1)**2)
def build_tile(x,y,f_cost,parent):
    return {"x":x,"y":y,"f":f_cost,"parent":parent}


def main(args=None) -> None:
    rclpy.init(args=args)
    node = AStarCalculator()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()

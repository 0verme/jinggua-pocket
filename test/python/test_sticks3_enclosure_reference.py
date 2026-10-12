import hashlib
import math
import re
import struct
import unittest
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CAD_DIR = ROOT / "cad" / "issue-15"
MODEL_PATH = CAD_DIR / "reference" / "StickS3.stl"
LICENSE_PATH = CAD_DIR / "reference" / "LICENSE"
SCAD_PATH = CAD_DIR / "sticks3_bumper.scad"
EXPECTED_MODEL_SHA256 = (
    "cadfa88716cb74a85d0d76dc866d83dbd627d6f5c310b528d43e588dab37ecec"
)


def load_binary_stl(path):
    data = path.read_bytes()
    if len(data) < 84:
        raise AssertionError("STL is shorter than a binary STL header")
    triangle_count = struct.unpack_from("<I", data, 80)[0]
    expected_size = 84 + 50 * triangle_count
    if len(data) != expected_size:
        raise AssertionError("STL size does not match its binary triangle count")
    triangles = []
    offset = 84
    for _ in range(triangle_count):
        vertices = []
        for vertex_offset in (12, 24, 36):
            vertices.append(struct.unpack_from("<fff", data, offset + vertex_offset))
        triangles.append(vertices)
        offset += 50
    return data, triangles


def face_bounds(faces):
    vertices = [vertex for face in faces for vertex in face]
    return tuple(
        (min(vertex[axis] for vertex in vertices),
         max(vertex[axis] for vertex in vertices))
        for axis in range(3)
    )


def mesh_components(triangles):
    parent = list(range(len(triangles)))
    rank = [0] * len(triangles)

    def find(index):
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    def union(left, right):
        left = find(left)
        right = find(right)
        if left == right:
            return
        if rank[left] < rank[right]:
            left, right = right, left
        parent[right] = left
        if rank[left] == rank[right]:
            rank[left] += 1

    owner = {}
    for face_index, face in enumerate(triangles):
        for vertex in face:
            key = tuple(round(value, 4) for value in vertex)
            if key in owner:
                union(face_index, owner[key])
            else:
                owner[key] = face_index

    components = defaultdict(list)
    for face_index, face in enumerate(triangles):
        components[find(face_index)].append(face)
    return list(components.values())


def planar_boundary_boxes(faces, plane_z):
    surface = [
        face
        for face in faces
        if all(abs(vertex[2] - plane_z) < 1e-4 for vertex in face)
    ]
    edge_counts = Counter()
    for face in surface:
        vertices = [tuple(round(value, 4) for value in vertex[:2]) for vertex in face]
        for index in range(3):
            edge = tuple(sorted((vertices[index], vertices[(index + 1) % 3])))
            edge_counts[edge] += 1

    adjacency = defaultdict(list)
    for (left, right), count in edge_counts.items():
        if count == 1:
            adjacency[left].append(right)
            adjacency[right].append(left)

    boxes = []
    visited = set()
    for start in adjacency:
        if start in visited:
            continue
        stack = [start]
        points = []
        visited.add(start)
        while stack:
            point = stack.pop()
            points.append(point)
            for neighbor in adjacency[point]:
                if neighbor not in visited:
                    visited.add(neighbor)
                    stack.append(neighbor)
        boxes.append(
            (
                min(point[0] for point in points),
                max(point[0] for point in points),
                min(point[1] for point in points),
                max(point[1] for point in points),
            )
        )
    return boxes


def scad_array(name):
    source = SCAD_PATH.read_text(encoding="utf-8")
    match = re.search(rf"(?m)^\s*{re.escape(name)}\s*=\s*\[([^]]+)\]\s*;", source)
    if not match:
        raise AssertionError(f"SCAD array {name!r} was not found")
    return [float(value.strip()) for value in match.group(1).split(",")]


def scad_scalar(name):
    source = SCAD_PATH.read_text(encoding="utf-8")
    match = re.search(rf"(?m)^\s*{re.escape(name)}\s*=\s*([0-9.]+)\s*;", source)
    if not match:
        raise AssertionError(f"SCAD scalar {name!r} was not found")
    return float(match.group(1))


class StickS3EnclosureReferenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data, cls.triangles = load_binary_stl(MODEL_PATH)
        cls.components = mesh_components(cls.triangles)

    def test_vendor_model_is_pinned_and_licensed(self):
        self.assertEqual(hashlib.sha256(self.data).hexdigest(), EXPECTED_MODEL_SHA256)
        self.assertEqual(len(self.triangles), 14696)
        license_text = LICENSE_PATH.read_text(encoding="utf-8")
        self.assertIn("MIT License", license_text)
        self.assertIn("Copyright (c) 2021 M5Stack", license_text)

    def test_first_device_group_matches_nominal_envelope(self):
        first_group = [
            face
            for face in self.triangles
            if all(-0.01 <= vertex[0] <= 24.02 for vertex in face)
        ]
        bounds = face_bounds(first_group)
        extents = [high - low for low, high in bounds]
        for actual, expected in zip(extents, (24.0, 48.0, 15.0)):
            self.assertAlmostEqual(actual, expected, delta=0.01)

    def test_front_apertures_are_carried_into_scad_parameters(self):
        front = None
        for component in self.components:
            bounds = face_bounds(component)
            x_bounds, y_bounds, z_bounds = bounds
            if (
                x_bounds[0] > -0.01
                and x_bounds[1] < 24.02
                and y_bounds[0] > 0.0
                and y_bounds[1] > 47.0
                and -10.2 < z_bounds[0] < -9.9
                and -0.1 < z_bounds[1] < 0.0
            ):
                front = component
                break
        self.assertIsNotNone(front, "front-face mesh component was not identified")

        front_z = max(vertex[2] for face in front for vertex in face)
        boxes = planar_boundary_boxes(front, front_z)
        expected_screen = (3.985, 20.015, 17.9246, 44.0157)
        expected_button = (6.030, 17.970, 9.3907, 11.7307)
        for expected in (expected_screen, expected_button):
            self.assertTrue(
                any(
                    all(math.isclose(actual, target, abs_tol=0.001)
                        for actual, target in zip(box, expected))
                    for box in boxes
                ),
                f"reference aperture {expected!r} was not found in {boxes!r}",
            )

        screen_size = scad_array("screen_nominal")
        screen_center = scad_array("screen_center")
        button_size = scad_array("button_nominal")
        button_center = scad_array("button_center")
        self.assertAlmostEqual(screen_size[0], expected_screen[1] - expected_screen[0], places=3)
        self.assertAlmostEqual(screen_size[1], expected_screen[3] - expected_screen[2], places=3)
        self.assertAlmostEqual(screen_center[0], (expected_screen[0] + expected_screen[1]) / 2, places=3)
        self.assertAlmostEqual(screen_center[1], (expected_screen[2] + expected_screen[3]) / 2, places=3)
        self.assertAlmostEqual(button_size[0], expected_button[1] - expected_button[0], places=3)
        self.assertAlmostEqual(button_size[1], expected_button[3] - expected_button[2], places=3)
        self.assertAlmostEqual(button_center[0], (expected_button[0] + expected_button[1]) / 2, places=3)
        self.assertAlmostEqual(button_center[1], (expected_button[2] + expected_button[3]) / 2, places=3)

    def test_prototype_clearance_and_wall_are_explicit(self):
        self.assertAlmostEqual(scad_scalar("fit_clearance"), 0.35)
        self.assertAlmostEqual(scad_scalar("wall_thickness"), 1.20)
        source = SCAD_PATH.read_text(encoding="utf-8")
        self.assertIn('import("reference/StickS3.stl"', source)
        self.assertIn('part = "preview"', source)


if __name__ == "__main__":
    unittest.main()

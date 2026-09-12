"""CPU regression: grouping, valid-value statistics, coordinates and old formats."""
import csv
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import streamsim_statistics as stats


class ComponentStatisticsTests(unittest.TestCase):
    def test_voxel_groups_and_sampling(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = np.array([[0, 0, 0, 10, 1, 300], [1, 0, 0, 30, 1, 302],
                             [2, 0, 0, 40, 2, 310], [3, 0, 0, np.nan, 3, 320],
                             [4, 0, 0, 9999, 4, 350]], dtype='<f4')
            data.tofile(root / 'data.bin')
            metadata = {'kind': 'voxel-energy-process', 'geometry': 'voxel', 'voxelCount': 5,
                        'recordFloats': 6, 'dataFile': 'data.bin', 'componentOffset': 4,
                        'fields': [{'id': 'sensibleHeat', 'label': 'H [W m-2]', 'offset': 3},
                                   {'id': 'temperature', 'label': '温度 [K]', 'offset': 5}]}
            source = root / 'node.json'
            source.write_text(json.dumps(metadata), encoding='utf-8')
            destination = root / 'out'
            destination.mkdir()
            stats.analyze_structure(source, destination, None, 'mean', 1)
            with (destination / 'components/component_statistics.csv').open(encoding='utf-8-sig') as stream:
                rows = list(csv.DictReader(stream))
            values = {(row['component'], row['field_id']): row for row in rows}
            self.assertEqual(float(values['soil', 'sensibleHeat']['mean']), 20)
            self.assertEqual(int(values['soil', 'temperatureC']['count']), 2)
            self.assertAlmostEqual(float(values['soil', 'temperatureC']['mean']), 27.85)
            self.assertEqual(int(values['building', 'sensibleHeat']['invalid_count']), 1)
            self.assertNotIn('photovoltaic', {row['component'] for row in rows})
            with (destination / 'components/vegetation/sample_points.csv').open(encoding='utf-8-sig') as stream:
                points = list(csv.DictReader(stream))
            self.assertEqual(points[0]['source_index'], '2')
            self.assertEqual(points[0]['x'], '2')
            self.assertEqual(points[0]['component'], '植被')

    def test_facets_and_legacy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'components.bin').write_bytes(bytes([1, 2, 3]))
            geometry = {'facetCount': 3, 'vertexPositions': [0, 0, 0, 3, 0, 0, 0, 3, 0] * 3,
                        'componentFile': 'components.bin'}
            (root / 'geometry.json').write_text(json.dumps(geometry), encoding='utf-8')
            np.array([[290], [310], [300], [320], [310], [330]], dtype='<f4').tofile(root / 'node.bin')
            metadata = {'kind': 'facet-energy-process', 'geometry': 'facet', 'facetCount': 3,
                        'geometryFile': 'geometry.json', 'dataFile': 'node.bin', 'recordFloats': 1,
                        'fields': [{'id': 'temperature', 'label': '温度 [K]', 'offset': 0}]}
            source = root / 'node.json'
            source.write_text(json.dumps(metadata), encoding='utf-8')
            for side, expected in [('front', [290, 300, 310]), ('back', [310, 320, 330]), ('mean', [300, 310, 320])]:
                structure = stats.read_structure(source, side)
                np.testing.assert_equal(structure.components, [1, 2, 3])
                np.testing.assert_equal(structure.positions[0], [1, 1, 0])
                np.testing.assert_equal(structure.fields[0].values, expected)
            geometry.pop('componentFile')
            (root / 'geometry.json').write_text(json.dumps(geometry), encoding='utf-8')
            self.assertIsNone(stats.read_structure(source, 'mean').components)


if __name__ == '__main__':
    unittest.main()

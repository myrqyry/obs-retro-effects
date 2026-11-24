import pathlib
import tempfile
import unittest
from check_readme_links import missing_targets


class ReadmeLinksTest(unittest.TestCase):
    def test_relative_links_and_images(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "docs").mkdir()
            (root / "docs/a b.png").touch()
            (root / "docs/guide.md").touch()
            readme = root / "README.md"
            readme.write_text(
                '[guide](docs/guide.md#install)\n'
                '![image](docs/a%20b.png)\n'
                '<img src="docs/a b.png">\n'
                '[web](https://example.com)\n'
                '[anchor](#start)\n'
                '~~~sh\n[not a link](missing-code)\n~~~\n'
                '[missing](docs/missing.md)\n'
                '<img src="docs/missing.png">\n'
            )
            self.assertEqual(missing_targets(readme),
                             ["docs/missing.md", "docs/missing.png"])


if __name__ == "__main__":
    unittest.main()

"""Generate the custom SettingsPagesModel.qml without changing QGC's page list."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path
from typing import Any

from tools.generators.settings_qml.emit import generate_pages_model_qml


def _load_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as stream:
        value = json.load(stream)

    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected a JSON object")
    return value


def _validate_pages(pages: object, source: Path) -> list[dict[str, Any]]:
    if not isinstance(pages, list):
        raise ValueError(f"{source}: 'pages' must be a list")

    result: list[dict[str, Any]] = []
    for index, page in enumerate(pages):
        if not isinstance(page, dict):
            raise ValueError(f"{source}: pages[{index}] must be an object")
        if not page.get("divider"):
            for key in ("name", "icon"):
                if not isinstance(page.get(key), str) or not page[key]:
                    raise ValueError(f"{source}: pages[{index}].{key} must be a non-empty string")
            if not isinstance(page.get("url") or page.get("qml"), str):
                raise ValueError(f"{source}: pages[{index}] must define 'url' or 'qml'")
        result.append(page)
    return result


def _merge_pages(base: dict[str, Any], custom: dict[str, Any], source: Path) -> dict[str, Any]:
    base_pages = _validate_pages(base.get("pages"), source)
    custom_pages = _validate_pages(custom.get("pages"), source)
    insert_after = custom.get("insertAfter")
    if not isinstance(insert_after, str) or not insert_after:
        raise ValueError(f"{source}: 'insertAfter' must be a non-empty string")

    matches = [index for index, page in enumerate(base_pages) if page.get("name") == insert_after]
    if len(matches) != 1:
        raise ValueError(
            f"{source}: expected exactly one base page named {insert_after!r}, found {len(matches)}"
        )

    existing_names = {page.get("name") for page in base_pages if page.get("name")}
    custom_names = [page["name"] for page in custom_pages if not page.get("divider")]
    duplicate_names = existing_names.intersection(custom_names)
    if duplicate_names or len(custom_names) != len(set(custom_names)):
        duplicates = sorted(
            duplicate_names or {name for name in custom_names if custom_names.count(name) > 1}
        )
        raise ValueError(f"{source}: duplicate page name(s): {', '.join(duplicates)}")

    insertion_index = matches[0] + 1
    merged = dict(base)
    merged["pages"] = base_pages[:insertion_index] + custom_pages + base_pages[insertion_index:]
    return merged


def generate(base_pages_dir: Path, custom_pages_path: Path, output_path: Path) -> None:
    base_pages_path = base_pages_dir / "SettingsPages.json"
    merged = _merge_pages(
        _load_json(base_pages_path),
        _load_json(custom_pages_path),
        custom_pages_path,
    )

    staging_dir = output_path.parent / "settings-pages-input"
    staging_dir.mkdir(parents=True, exist_ok=True)
    for source in base_pages_dir.glob("*.json"):
        if source.name != base_pages_path.name:
            shutil.copy2(source, staging_dir / source.name)

    staged_pages_path = staging_dir / base_pages_path.name
    staged_pages_path.write_text(json.dumps(merged, indent=4) + "\n", encoding="utf-8")
    generated_qml = generate_pages_model_qml(staged_pages_path)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(generated_qml, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-pages-dir", type=Path, required=True)
    parser.add_argument("--custom-pages", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    generate(args.base_pages_dir, args.custom_pages, args.output)


if __name__ == "__main__":
    main()

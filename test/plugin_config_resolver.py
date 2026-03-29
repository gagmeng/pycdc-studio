from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


@dataclass(slots=True)
class PluginSpec:
    name: str
    enabled: bool = True
    entrypoint: str = "run"
    config: dict[str, Any] = field(default_factory=dict)


@dataclass(slots=True)
class ConfigBundle:
    root: Path
    env: str
    plugins: list[PluginSpec] = field(default_factory=list)
    variables: dict[str, Any] = field(default_factory=dict)


class ConfigResolver:
    def __init__(self, bundle: ConfigBundle) -> None:
        self.bundle = bundle
        self._warnings: list[str] = []

    def resolve(self) -> dict[str, Any]:
        resolved_plugins = [self._resolve_plugin(plugin) for plugin in self.bundle.plugins if plugin.enabled]
        paths = self._expand_paths(self.bundle.variables.get("paths", []))
        summary = self._build_summary(resolved_plugins)
        return {
            "root": str(self.bundle.root),
            "env": self.bundle.env,
            "plugins": resolved_plugins,
            "paths": paths,
            "summary": summary,
            "warnings": list(self._warnings),
        }

    def diff(self, other: ConfigBundle) -> dict[str, Any]:
        current = self.resolve()
        other_resolver = ConfigResolver(other)
        target = other_resolver.resolve()
        changed_keys = sorted({*current.keys(), *target.keys()})
        return {
            key: {"current": current.get(key), "target": target.get(key)}
            for key in changed_keys
            if current.get(key) != target.get(key)
        }

    def _resolve_plugin(self, plugin: PluginSpec) -> dict[str, Any]:
        defaults = self._default_plugin_config(plugin.name)
        merged = {**defaults, **plugin.config}
        merged["entrypoint"] = plugin.entrypoint
        merged["qualified_name"] = f"plugins.{plugin.name}:{plugin.entrypoint}"
        merged["features"] = self._normalize_features(merged.get("features", []))

        if merged.get("timeout", 0) <= 0:
            self._warnings.append(f"plugin {plugin.name} has a non-positive timeout")

        return merged

    def _default_plugin_config(self, name: str) -> dict[str, Any]:
        match name:
            case "image":
                return {"timeout": 10, "features": ["thumbnail", "optimize"], "format": "png"}
            case "notify":
                return {"timeout": 3, "features": ["desktop"], "channel": "system"}
            case "archive":
                return {"timeout": 15, "features": ["compress", "checksum"], "format": "zip"}
            case _:
                return {"timeout": 5, "features": ["default"]}

    def _normalize_features(self, features: Iterable[Any]) -> tuple[str, ...]:
        normalized: list[str] = []
        for feature in features:
            if feature is None:
                continue
            name = str(feature).strip().lower()
            if not name:
                continue
            if name not in normalized:
                normalized.append(name)
        return tuple(normalized)

    def _expand_paths(self, values: Iterable[Any]) -> list[str]:
        expanded: list[str] = []
        for value in values:
            match value:
                case str() if value.startswith("~/"):
                    expanded.append(str(Path.home() / value[2:]))
                case str() if value.startswith("./"):
                    expanded.append(str(self.bundle.root / value[2:]))
                case str():
                    expanded.append(value)
                case _:
                    expanded.append(str(value))
        return expanded

    def _build_summary(self, plugins: list[dict[str, Any]]) -> dict[str, Any]:
        feature_usage: dict[str, int] = {}
        for plugin in plugins:
            for feature in plugin.get("features", ()): 
                feature_usage[feature] = feature_usage.get(feature, 0) + 1
        return {
            "plugin_count": len(plugins),
            "feature_usage": dict(sorted(feature_usage.items())),
            "env_label": self.bundle.env.upper(),
        }


def build_sample_bundle() -> ConfigBundle:
    return ConfigBundle(
        root=Path("D:/code/pycdc-studio/test"),
        env="dev",
        plugins=[
            PluginSpec("image", config={"format": "webp", "features": ["optimize", "preview", "preview"]}),
            PluginSpec("notify", config={"channel": "slack", "timeout": 2}),
            PluginSpec("archive", enabled=True, config={"features": ["compress", None, "checksum"]}),
        ],
        variables={
            "paths": ["./assets", "~/Downloads", "C:/temp"],
            "team": "core-platform",
        },
    )


if __name__ == "__main__":
    base = build_sample_bundle()
    other = ConfigBundle(
        root=base.root,
        env="prod",
        plugins=[
            PluginSpec("image", config={"format": "png", "features": ["thumbnail"]}),
            PluginSpec("notify", enabled=False),
        ],
        variables={"paths": ["./assets", "./archive"]},
    )
    resolver = ConfigResolver(base)
    print(resolver.resolve())
    print(resolver.diff(other))

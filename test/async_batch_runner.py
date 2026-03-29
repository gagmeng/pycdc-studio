from __future__ import annotations

import asyncio
from collections import defaultdict
from dataclasses import dataclass
from typing import Any


@dataclass(slots=True)
class Job:
    name: str
    delay: float
    payload: dict[str, Any]


class JobTimeoutError(RuntimeError):
    pass


class AsyncJobRunner:
    def __init__(self, concurrency: int = 3) -> None:
        self._concurrency = concurrency
        self._history: list[dict[str, Any]] = []

    async def run_batch(self, jobs: list[Job]) -> dict[str, Any]:
        semaphore = asyncio.Semaphore(self._concurrency)
        grouped_results: defaultdict[str, list[dict[str, Any]]] = defaultdict(list)

        async def run_single(job: Job) -> None:
            async with semaphore:
                result = await self._execute_job(job)
                grouped_results[result["status"]].append(result)
                self._history.append(result)

        await asyncio.gather(*(run_single(job) for job in jobs))
        return {
            "total": len(jobs),
            "ok": len(grouped_results["ok"]),
            "timeout": len(grouped_results["timeout"]),
            "error": len(grouped_results["error"]),
            "results": dict(grouped_results),
        }

    async def collect_metrics(self) -> dict[str, Any]:
        buckets: dict[str, int] = {"ok": 0, "timeout": 0, "error": 0}
        durations: list[float] = []
        for item in self._history:
            buckets[item["status"]] = buckets.get(item["status"], 0) + 1
            durations.append(item.get("duration", 0.0))
        average_duration = sum(durations) / len(durations) if durations else 0.0
        return {
            "counts": buckets,
            "average_duration": round(average_duration, 4),
            "history_size": len(self._history),
        }

    async def replay_failures(self) -> list[dict[str, Any]]:
        failures = [item for item in self._history if item["status"] != "ok"]
        replayed: list[dict[str, Any]] = []
        for item in failures:
            cloned = Job(
                name=f"replay::{item['job']}",
                delay=0.01,
                payload={"mode": "safe", "source": item},
            )
            replayed.append(await self._execute_job(cloned))
        return replayed

    async def _execute_job(self, job: Job) -> dict[str, Any]:
        start = asyncio.get_running_loop().time()
        try:
            data = await self._fetch(job)
            transformed = self._transform(job, data)
            await self._store(job, transformed)
            return {
                "job": job.name,
                "status": "ok",
                "duration": asyncio.get_running_loop().time() - start,
                "items": len(transformed),
            }
        except JobTimeoutError as exc:
            return {
                "job": job.name,
                "status": "timeout",
                "duration": asyncio.get_running_loop().time() - start,
                "error": str(exc),
            }
        except Exception as exc:  # noqa: BLE001
            return {
                "job": job.name,
                "status": "error",
                "duration": asyncio.get_running_loop().time() - start,
                "error": str(exc),
            }

    async def _fetch(self, job: Job) -> list[dict[str, Any]]:
        await asyncio.sleep(job.delay)
        mode = job.payload.get("mode", "normal")
        match mode:
            case "timeout":
                raise JobTimeoutError(f"job {job.name} timed out")
            case "error":
                raise ValueError(f"job {job.name} failed during fetch")
            case _:
                return [
                    {"key": key, "value": value, "job": job.name}
                    for key, value in job.payload.items()
                    if key != "mode"
                ]

    def _transform(self, job: Job, data: list[dict[str, Any]]) -> list[dict[str, Any]]:
        def normalize(item: dict[str, Any]) -> dict[str, Any]:
            value = item["value"]
            if isinstance(value, str):
                normalized_value: Any = value.upper()
            elif isinstance(value, (int, float)):
                normalized_value = value * 2
            elif value is None:
                normalized_value = "NONE"
            else:
                normalized_value = repr(value)
            return {
                **item,
                "value": normalized_value,
                "kind": type(value).__name__,
                "tags": tuple(sorted(job.payload.get("tags", []))),
            }

        return [normalize(item) for item in data]

    async def _store(self, job: Job, rows: list[dict[str, Any]]) -> None:
        await asyncio.sleep(0)
        if job.payload.get("persist") is False and len(rows) > 3:
            raise RuntimeError(f"job {job.name} refuses to store large payloads")


async def demo() -> None:
    runner = AsyncJobRunner(concurrency=2)
    jobs = [
        Job("users", 0.02, {"source": "users", "limit": 20, "tags": ["daily", "critical"]}),
        Job("slow-report", 0.01, {"mode": "timeout", "source": "report"}),
        Job("metrics", 0.03, {"source": "metrics", "tags": ["background"], "persist": True}),
        Job("broken", 0.01, {"mode": "error", "source": "orders"}),
    ]
    print(await runner.run_batch(jobs))
    print(await runner.collect_metrics())
    print(await runner.replay_failures())


if __name__ == "__main__":
    asyncio.run(demo())

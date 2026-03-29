from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timedelta
from functools import wraps
from typing import Any, Callable


@dataclass(slots=True)
class TaskSpec:
    name: str
    payload: dict[str, Any]
    retries: int = 0
    tags: tuple[str, ...] = ()


@dataclass(slots=True)
class ExecutionRecord:
    task_name: str
    started_at: datetime
    finished_at: datetime | None = None
    status: str = "pending"
    attempts: int = 0
    details: dict[str, Any] = field(default_factory=dict)


class WorkflowError(RuntimeError):
    pass


class RetryableWorkflowError(WorkflowError):
    pass


class FatalWorkflowError(WorkflowError):
    pass


def audit(event_name: str) -> Callable[[Callable[..., Any]], Callable[..., Any]]:
    def decorator(func: Callable[..., Any]) -> Callable[..., Any]:
        @wraps(func)
        def wrapper(*args: Any, **kwargs: Any) -> Any:
            result = func(*args, **kwargs)
            if isinstance(result, dict):
                result.setdefault("events", []).append(event_name)
            return result

        return wrapper

    return decorator


class WorkflowEngine:
    def __init__(self) -> None:
        self._records: list[ExecutionRecord] = []
        self._handlers: dict[str, Callable[[TaskSpec], dict[str, Any]]] = {
            "email": self._handle_email,
            "sync": self._handle_sync,
            "report": self._handle_report,
        }

    def records(self) -> list[ExecutionRecord]:
        return list(self._records)

    def register_handler(self, name: str, handler: Callable[[TaskSpec], dict[str, Any]]) -> None:
        self._handlers[name] = handler

    def build_plan(self, tasks: list[TaskSpec]) -> list[tuple[str, str]]:
        plan: list[tuple[str, str]] = []
        for task in tasks:
            category = self._classify_task(task)
            plan.append((task.name, category))
        return plan

    def execute(self, tasks: list[TaskSpec]) -> list[dict[str, Any]]:
        results: list[dict[str, Any]] = []
        for task in tasks:
            record = ExecutionRecord(task_name=task.name, started_at=datetime.utcnow())
            self._records.append(record)
            try:
                self._validate_task(task)
                handler = self._resolve_handler(task)
                result = self._run_with_retry(task, handler, record)
                record.status = "ok"
                results.append(result)
            except RetryableWorkflowError as exc:
                record.status = "retry_exhausted"
                record.details["error"] = str(exc)
                results.append({"task": task.name, "status": "retry_exhausted", "error": str(exc)})
            except FatalWorkflowError as exc:
                record.status = "fatal"
                record.details["error"] = str(exc)
                results.append({"task": task.name, "status": "fatal", "error": str(exc)})
            finally:
                record.finished_at = datetime.utcnow()
                record.details["duration_ms"] = int((record.finished_at - record.started_at).total_seconds() * 1000)
        return results

    def summarize(self) -> dict[str, int]:
        summary = {"ok": 0, "retry_exhausted": 0, "fatal": 0, "pending": 0}
        for record in self._records:
            summary.setdefault(record.status, 0)
            summary[record.status] += 1
        return summary

    def prune_old_records(self, horizon: timedelta) -> int:
        cutoff = datetime.utcnow() - horizon
        before = len(self._records)
        self._records = [record for record in self._records if record.started_at >= cutoff]
        return before - len(self._records)

    def _resolve_handler(self, task: TaskSpec) -> Callable[[TaskSpec], dict[str, Any]]:
        kind = task.payload.get("kind", task.name)
        if kind not in self._handlers:
            raise FatalWorkflowError(f"unknown task kind: {kind}")
        return self._handlers[kind]

    def _run_with_retry(
        self,
        task: TaskSpec,
        handler: Callable[[TaskSpec], dict[str, Any]],
        record: ExecutionRecord,
    ) -> dict[str, Any]:
        last_error: Exception | None = None
        for attempt in range(task.retries + 1):
            record.attempts += 1
            try:
                result = handler(task)
                result["attempt"] = attempt + 1
                result["task"] = task.name
                return result
            except RetryableWorkflowError as exc:
                last_error = exc
                record.details.setdefault("attempt_errors", []).append(str(exc))
        raise RetryableWorkflowError(str(last_error) if last_error else "retry budget exceeded")

    def _validate_task(self, task: TaskSpec) -> None:
        if not task.name.strip():
            raise FatalWorkflowError("task name cannot be empty")
        if not isinstance(task.payload, dict):
            raise FatalWorkflowError("task payload must be a mapping")
        priority = task.payload.get("priority", "normal")
        if priority not in {"low", "normal", "high"}:
            raise FatalWorkflowError(f"invalid priority: {priority}")

    def _classify_task(self, task: TaskSpec) -> str:
        match task.payload.get("priority", "normal"):
            case "high":
                return "urgent"
            case "low":
                return "background"
            case _ if "scheduled" in task.tags:
                return "scheduled"
            case _:
                return "default"

    @audit("email_sent")
    def _handle_email(self, task: TaskSpec) -> dict[str, Any]:
        recipient = task.payload.get("recipient")
        if not recipient:
            raise FatalWorkflowError("missing recipient")
        subject = task.payload.get("subject", "no-subject")
        return {"status": "ok", "recipient": recipient, "subject": subject}

    def _handle_sync(self, task: TaskSpec) -> dict[str, Any]:
        source = task.payload.get("source")
        if source == "unstable":
            raise RetryableWorkflowError("temporary sync failure")
        if source is None:
            raise FatalWorkflowError("missing sync source")
        items = task.payload.get("items", [])
        normalized = {str(index): value for index, value in enumerate(items) if value is not None}
        return {"status": "ok", "synced": len(normalized), "normalized": normalized}

    def _handle_report(self, task: TaskSpec) -> dict[str, Any]:
        sections = task.payload.get("sections", [])

        def build_section_map() -> dict[str, int]:
            return {str(section.get("name", index)): len(section.get("rows", [])) for index, section in enumerate(sections)}

        return {
            "status": "ok",
            "section_count": len(sections),
            "rows_per_section": build_section_map(),
        }


if __name__ == "__main__":
    engine = WorkflowEngine()
    samples = [
        TaskSpec("email", {"kind": "email", "recipient": "team@example.com", "subject": "daily"}),
        TaskSpec("sync", {"kind": "sync", "source": "primary", "items": [1, None, 3]}, retries=1),
        TaskSpec("report", {"kind": "report", "sections": [{"name": "errors", "rows": [1, 2, 3]}]}),
    ]
    print(engine.build_plan(samples))
    print(engine.execute(samples))
    print(engine.summarize())

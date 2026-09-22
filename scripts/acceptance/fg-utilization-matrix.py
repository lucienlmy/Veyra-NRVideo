"""Sequential product UI loads with read-only NVML device telemetry."""
import argparse
import ctypes as c
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

sys.dont_write_bytecode = True


class Utilization(c.Structure):
    _fields_ = [("gpu", c.c_uint), ("memory", c.c_uint)]


class Memory(c.Structure):
    _fields_ = [("total", c.c_ulonglong), ("free", c.c_ulonglong), ("used", c.c_ulonglong)]


class Nvml:
    def __init__(self):
        self.dll = c.CDLL(str(Path(os.environ["SystemRoot"]) / "System32/nvml.dll"))
        self.call("nvmlInit_v2")
        self.device = c.c_void_p()
        self.call("nvmlDeviceGetHandleByIndex_v2", c.c_uint(0), c.byref(self.device))

    def call(self, name, *args):
        code = getattr(self.dll, name)(*args)
        if code:
            raise RuntimeError(f"{name} returned {code}")

    def scalar(self, name, *args):
        result = c.c_uint()
        self.call(name, self.device, *args, c.byref(result))
        return result.value

    def sample(self):
        util, memory = Utilization(), Memory()
        self.call("nvmlDeviceGetUtilizationRates", self.device, c.byref(util))
        self.call("nvmlDeviceGetMemoryInfo", self.device, c.byref(memory))
        return dict(host100ns=time.perf_counter_ns() // 100,
                    gpu=util.gpu, memoryBusy=util.memory,
                    powerW=self.scalar("nvmlDeviceGetPowerUsage") / 1000,
                    smMHz=self.scalar("nvmlDeviceGetClockInfo", c.c_uint(1)),
                    memoryMHz=self.scalar("nvmlDeviceGetClockInfo", c.c_uint(2)),
                    temperatureC=self.scalar("nvmlDeviceGetTemperature", c.c_uint(0)),
                    pstate=self.scalar("nvmlDeviceGetPerformanceState"),
                    memoryMiB=memory.used / 1048576)

    def close(self):
        self.call("nvmlShutdown")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--derived", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--temp", type=Path, required=True)
    parser.add_argument("--cases", nargs="*")
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--temporal", action="store_true")
    parser.add_argument("--sr-backend", choices=("video", "dlss"), default="video")
    args = parser.parse_args()
    if not 10 <= args.seconds <= 240:
        parser.error("Each run must be 10..240 seconds")
    spec = importlib.util.spec_from_file_location("cadence", Path(__file__).with_name("analyze-fg-cadence.py"))
    cadence = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cadence)
    cases = {}
    for source in ("native", "sr"):
        for nr in (False, True):
            for backend, multipliers in (("dlss", (1, 2, 4, 6)), ("xess", (2, 4))):
                for multiplier in multipliers:
                    name = f"{source}-{'nr' if nr else 'plain'}-{backend}{multiplier}"
                    cases[name] = (source, nr, backend, multiplier)
    selected = args.cases or list(cases)
    if any(name not in cases for name in selected):
        parser.error("Unknown case")
    args.output.mkdir(parents=True, exist_ok=True)
    args.temp.mkdir(parents=True, exist_ok=True)
    with args.exe.open("rb") as executable:
        executable_hash = hashlib.file_digest(executable, "sha256").hexdigest()
    nvml = Nvml()
    summaries = []
    try:
        for name in selected:
            out = args.output / name
            out.mkdir()  # Never overwrite previous evidence.
            source, nr, backend, multiplier = cases[name]
            env = os.environ.copy()
            env.update(TEMP=str(args.temp.resolve()), TMP=str(args.temp.resolve()),
                       VEYRA_LOG_FILE=str((out / "app.log").resolve()),
                       VEYRA_SMOKE_TRACE_FILE=str((out / "trace.txt").resolve()),
                       VEYRA_TEST_TRACE_SUBFRAMES="1")
            argv = [str(args.exe.resolve()), str((args.native if source == "native" else args.derived).resolve()),
                    "--smoke-view", "pro", "--smoke-seconds", str(args.seconds),
                    "--nr" if nr else "--no-nr", "--realtime"]
            argv += (["--no-sr"] if source == "native" else
                     ["--video-sr", "3"] if args.sr_backend == "video" else ["--sr"])
            argv += ["--no-fg"] if multiplier == 1 else [f"--fg-{backend}", "--fg-multiplier", str(multiplier)]
            if args.temporal:
                argv += ["--nr-temporal"]
            samples = []
            started = time.perf_counter()
            with (out / "stdout.log").open("w", encoding="utf-8") as stdout, (out / "stderr.log").open("w", encoding="utf-8") as stderr:
                process = subprocess.Popen(argv, cwd=args.exe.parent, env=env, stdout=stdout, stderr=stderr)
                try:
                    while process.poll() is None:
                        elapsed = time.perf_counter() - started
                        if elapsed > args.seconds + 45:
                            raise TimeoutError(f"{name} exceeded watchdog")
                        samples.append(dict(elapsed=elapsed, **nvml.sample()))
                        time.sleep(.2)
                finally:
                    if process.poll() is None:
                        process.kill()
                    process.wait(timeout=10)
            (out / "telemetry.json").write_text(json.dumps(samples), encoding="utf-8")
            retained = [sample for sample in samples if 10 <= sample["elapsed"] <= args.seconds - 1]
            result = dict(case=name, exit=process.returncode, argv=argv, srBackend=args.sr_backend,
                          executableSha256=executable_hash,
                          diagnosticEnvironment={key: value for key, value in env.items()
                                                 if key.startswith("VEYRA_TEST_") or key.startswith("VEYRA_DISABLE_")},
                          scope="NVML device-wide averaged activity, not SM occupancy or frame-scale GPU idle; software presentation is not scanout.",
                          telemetry={key: cadence.distribution([row[key] for row in retained])
                                     for key in ("gpu", "memoryBusy", "powerW", "smMHz", "temperatureC", "memoryMiB")})
            log = (out / "stdout.log").read_text(encoding="utf-8", errors="replace")
            timing = [dict(re.findall(r"(\w+)=([^ ]+)", line)) for line in log.splitlines() if "[player-timing]" in line]
            result["timingLast"] = timing[-1] if timing else None
            result["smoke"] = next((line for line in reversed(log.splitlines()) if "smoke frames=" in line), None)
            if (out / "trace.txt").exists():
                try:
                    result["cadence"] = cadence.analyze_trace(out / "trace.txt")
                except ValueError as error:
                    result["cadenceError"] = str(error)
            (out / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
            summaries.append(result)
            (args.output / "matrix.json").write_text(json.dumps(summaries, indent=2), encoding="utf-8")
            print(json.dumps({"case": name, "exit": process.returncode,
                              "gpuMean": result["telemetry"]["gpu"],
                              "submitFps": result.get("cadence", {}).get("submitFps"),
                              "smoke": result["smoke"]}), flush=True)
            if process.returncode:
                raise RuntimeError(f"{name} failed; inspect before continuing")
    finally:
        nvml.close()


if __name__ == "__main__":
    main()

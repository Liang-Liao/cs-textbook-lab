#!/usr/bin/env python3
"""
Copy prior-lab business algorithms (from labs/<owner>/src/) into labs/<target>/vendor/.

正本在各前序 lab 的 src/；vendor 是 copy，供本 lab 编译与阅读。
用法:
  python scripts/sync_vendor.py --lab B2-newton-quasi-tr
  python scripts/sync_vendor.py --all
"""
from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LABS = ROOT / "labs"

# target lab -> list of (owner_lab, [header basenames])
VENDOR_SPEC: dict[str, list[tuple[str, list[str]]]] = {
    "A1-numeric-la": [],
    "A2-prob-stat": [],
    "A3-convex-opt": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["rng.h", "dist.h", "stats.h", "gof.h"]),
    ],
    "B1-line-search-gd": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
    ],
    "B2-newton-quasi-tr": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["rng.h", "dist.h", "stats.h", "gof.h"]),
        ("A3-convex-opt", ["conv.h"]),
        ("B1-line-search-gd", ["linesearch.h", "opt.h"]),
    ],
    "B3-least-squares": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["dist.h", "stats.h", "rng.h"]),
    ],
    "B4-lp-ip": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["rng.h", "dist.h", "stats.h", "gof.h"]),
    ],
    "B5-constrained": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["rng.h", "dist.h", "stats.h", "gof.h"]),
        ("A3-convex-opt", ["conv.h"]),
        ("B1-line-search-gd", ["linesearch.h", "opt.h"]),
        ("B2-newton-quasi-tr", ["optcore.h", "bench.h"]),
    ],
    "B6-sgd-adam": [
        ("A1-numeric-la", ["vec.h"]),  # 范数
        ("A2-prob-stat", ["rng.h"]),   # shuffle/采样；dist/stats/gof 随 rng.c 的 include 链带入
    ],
    "B7-prox-admm": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["rng.h", "dist.h", "stats.h", "gof.h"]),
        ("B1-line-search-gd", ["linesearch.h", "opt.h"]),  # ADMM 与 B5 AL 对账所需链
        ("B2-newton-quasi-tr", ["optcore.h", "bench.h"]),
        ("B5-constrained", ["nlp.h"]),
        ("A3-convex-opt", ["conv.h"]),  # optcore 依赖 conv
    ],
    "C1-rng-mc": [
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
    ],
    # future labs: extend here, e.g. C2 vendors C1 rng/mc + A2 gof
    "C2-mcmc": [
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("C1-rng-mc", ["rng.h", "mc.h"]),
    ],
    "C3-sa": [
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("C1-rng-mc", ["rng.h", "mc.h"]),
        ("B1-line-search-gd", ["opt.h"]),
    ],
    "C4-ts-vns-ils": [
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("C1-rng-mc", ["rng.h", "mc.h"]),
        ("C3-sa", ["tsp.h", "sa.h"]),
    ],
    "C5-ga-de": [
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("C1-rng-mc", ["rng.h", "mc.h"]),
        ("C3-sa", ["bench_sa.h"]),
        ("B2-newton-quasi-tr", ["bench.h"]),
    ],
    "C6-es-cmaes": [
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("C1-rng-mc", ["rng.h", "mc.h"]),
        ("B2-newton-quasi-tr", ["bench.h"]),
        ("C3-sa", ["bench_sa.h"]),
        ("C5-ga-de", ["de.h"]),
    ],
    "C7-pso-aco": [
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("C1-rng-mc", ["rng.h", "mc.h"]),
        ("B2-newton-quasi-tr", ["bench.h"]),
        ("C3-sa", ["bench_sa.h", "tsp.h", "sa.h"]),
    ],
    "C8-constraint-mo": [
        # rng.c 实现依赖 dist/stats/gof（chi2/mean/quantile）；mc/bench/de 未使用（修复轮裁剪）
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("C1-rng-mc", ["rng.h"]),
    ],
    "C9-bayes-opt": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("A3-convex-opt", ["conv.h"]),
        ("C1-rng-mc", ["rng.h"]),
        ("B1-line-search-gd", ["opt.h", "linesearch.h"]),
        ("B2-newton-quasi-tr", ["optcore.h", "bench.h"]),
    ],
    "D1-interp-spline": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("C1-rng-mc", ["rng.h"]),
        # 修复轮 R6：kriging 与 GP 同源对照需要 C9 gp（D1 在 C9 之后）
        ("C9-bayes-opt", ["gp.h"]),
    ],
    "D2-ode-sim": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("C1-rng-mc", ["rng.h"]),
        ("B3-least-squares", ["lsq.h"]),
    ],
    "D3-graph-combo": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("C1-rng-mc", ["rng.h"]),
        ("B4-lp-ip", ["knapsack.h", "simplex.h"]),
        # 修复轮 R6：指派问题与 B4 联动（:649）
        ("B4-lp-ip", ["assign.h"]),
    ],
    "M1-curve-fitting": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("A3-convex-opt", ["conv.h"]),
        ("C1-rng-mc", ["rng.h"]),
        ("B1-line-search-gd", ["opt.h", "linesearch.h"]),
        ("B2-newton-quasi-tr", ["optcore.h", "bench.h"]),
        ("B3-least-squares", ["lsq.h"]),
    ],
    "M2-global-bench": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("A3-convex-opt", ["conv.h"]),
        ("C1-rng-mc", ["rng.h"]),
        ("B1-line-search-gd", ["opt.h", "linesearch.h"]),
        ("B2-newton-quasi-tr", ["optcore.h", "bench.h"]),
        ("C3-sa", ["sa.h", "bench_sa.h"]),
        ("C5-ga-de", ["de.h"]),
        ("C6-es-cmaes", ["cmaes.h", "es.h"]),
        ("C7-pso-aco", ["pso.h"]),
    ],
    "M3-e2e-param-est": [
        ("A1-numeric-la", ["vec.h", "mat.h", "linalg.h"]),
        ("A2-prob-stat", ["dist.h", "stats.h", "gof.h"]),
        ("A3-convex-opt", ["conv.h"]),
        ("C1-rng-mc", ["rng.h"]),
        ("B1-line-search-gd", ["opt.h", "linesearch.h"]),
        ("B2-newton-quasi-tr", ["optcore.h", "bench.h"]),
        ("C2-mcmc", ["mcmc.h"]),
        ("C5-ga-de", ["de.h"]),
        ("D2-ode-sim", ["ode.h"]),
    ],
}


def expand_files(owner: str, names: list[str]) -> list[str]:
    src = LABS / owner / "src"
    files: list[str] = []
    for n in names:
        files.append(n)
        if n.endswith(".h"):
            c = n[:-2] + ".c"
            if (src / c).is_file():
                files.append(c)
    extra: set[str] = set()
    for n in list(files):
        p = src / n
        if not p.is_file():
            continue
        for inc in re.findall(r'#include\s+"([^"]+)"', p.read_text(encoding="utf-8", errors="replace")):
            if inc.endswith(".h") and (src / inc).is_file():
                extra.add(inc)
                c = inc[:-2] + ".c"
                if (src / c).is_file():
                    extra.add(c)
    files.extend(sorted(extra))
    seen: set[str] = set()
    out: list[str] = []
    for f in files:
        if f not in seen:
            seen.add(f)
            out.append(f)
    return out


def sync_one(lab_id: str) -> int:
    spec = VENDOR_SPEC.get(lab_id)
    if spec is None:
        print(f"no VENDOR_SPEC for {lab_id}")
        return 1
    vendor = LABS / lab_id / "vendor"
    if vendor.exists():
        shutil.rmtree(vendor)
    vendor.mkdir(parents=True, exist_ok=True)
    lines = [
        "# vendor（前序 lab 业务算法 copy）",
        "",
        f"本 lab **{lab_id}** 编译使用本目录；正本在下列前序 lab 的 `src/`。",
        "只读：不要在 vendor 写业务代码。更新依赖时重跑 sync_vendor。",
        "",
    ]
    for owner, names in spec:
        files = expand_files(owner, names)
        copied = []
        for f in files:
            sp = LABS / owner / "src" / f
            if not sp.is_file():
                print(f"  WARN {lab_id}: missing {owner}/src/{f}")
                continue
            shutil.copy2(sp, vendor / f)
            copied.append(f)
        lines.append(f"- 来自 `{owner}/src`: {', '.join(copied)}")
    (vendor / "VENDOR.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    n = len([p for p in vendor.iterdir() if p.name != "VENDOR.md"])
    print(f"{lab_id}: vendor {n} files")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--lab", action="append")
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args()
    if args.lab:
        ids = args.lab
    elif args.all:
        # only labs that already exist on disk
        ids = [p.name for p in sorted(LABS.iterdir()) if p.is_dir()]
    else:
        ids = [k for k in VENDOR_SPEC if (LABS / k).is_dir()]
    rc = 0
    for lab_id in ids:
        if not (LABS / lab_id).is_dir():
            print(f"skip missing lab dir {lab_id}")
            continue
        rc |= sync_one(lab_id)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())

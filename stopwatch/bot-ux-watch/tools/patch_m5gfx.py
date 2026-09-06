Import("env")

import hashlib
import json
import subprocess
from pathlib import Path


EXPECTED_REVISION = "d91077b9a607b59404e4e4a49f775c792bfae382"
FILES = {
    "src/M5GFX.cpp": (
        "df1b2e3882c45d1aa7743f6f30081fc753c609cdf38f352143f7f3bc3738514e",
        "494903aa46742226bdd9a8b23fa65b4cb27784ddfeb20cbff96efdcb0be92d0c",
    ),
    "src/lgfx/v1/panel/Panel_AMOLED.cpp": (
        "8b71b03b51693a14890872e6918723e191976200fdeb9658d203363c862be4bf",
        "e95141b3ca53edd95a3d205cb8d9cb0063139fb35cb007959b02c4c76d18c2b4",
    ),
    "src/lgfx/v1/panel/Panel_AMOLED.hpp": (
        "d8cdc0b76ae951a8f2ce73f8128ecd40aae237c7485646fd218d24a91198f96e",
        "38a18ae04140c6306c79578d79f3c4469717027f9b16cdb5aec20be93add9ad5",
    ),
    "src/lgfx/v1/touch/Touch_CSTxxx.cpp": (
        "8f2d4e2dd56659ecd684471fdb4622a11f51c3691f7f9c53e4f93641e9cb3ead",
        "492f92a8c5f1051a2f1d96b372b71475596ffc48dba39b17746ec331312d3544",
    ),
    "src/lgfx/v1/touch/Touch_CSTxxx.hpp": (
        "4a89e6dd2fe8d81dec09a7b3907edf098d841b0786cc8432dec37195d69c1040",
        "52b919b32d61b91c8ba81c273fa878a5d84c2381e1964b7f65e703ce230c5211",
    ),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


deps = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
candidates = []
for directory in deps.glob("M5GFX*"):
    manifest = directory / "library.json"
    if not manifest.is_file() or json.loads(manifest.read_text()).get("name") != "M5GFX":
        continue
    try:
        revision = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=str(directory), text=True
        ).strip()
    except subprocess.CalledProcessError:
        continue
    if revision == EXPECTED_REVISION:
        candidates.append(directory)
if len(candidates) != 1:
    raise RuntimeError("Expected exactly one installed M5GFX dependency, found %d" % len(candidates))

library = candidates[0]
state = [digest(library / name) for name in FILES]
original = [hashes[0] for hashes in FILES.values()]
patched = [hashes[1] for hashes in FILES.values()]

if state == original:
    patch = Path(env.subst("$PROJECT_DIR")) / "patches" / "m5gfx-stopwatch-cst820.patch"
    subprocess.run(["git", "apply", "--unidiff-zero", "--check", str(patch)], cwd=str(library), check=True)
    subprocess.run(["git", "apply", "--unidiff-zero", str(patch)], cwd=str(library), check=True)
    state = [digest(library / name) for name in FILES]
    if state != patched:
        raise RuntimeError("M5GFX StopWatch patch produced unexpected content")
    print("Applied verified M5GFX StopWatch CST820 patch")
elif state == patched:
    print("Verified existing M5GFX StopWatch CST820 patch")
else:
    raise RuntimeError("Refusing to patch modified or unexpected M5GFX sources")

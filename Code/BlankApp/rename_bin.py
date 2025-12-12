Import("projenv")
import shutil, os

app_name = "tarot"  # ← CHANGE THIS to your app's filename

def after_build(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    src = os.path.join(build_dir, "firmware.bin")
    dst = os.path.join(build_dir, f"{app_name}.bin")
    if os.path.exists(src):
        shutil.copy(src, dst)
        print(f"Created: {dst}")

projenv.AddPostAction("$BUILD_DIR/firmware.bin", after_build)

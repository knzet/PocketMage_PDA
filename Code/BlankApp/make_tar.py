Import("projenv")
import subprocess, os

app_name = "tarot"   # change to your app name

def create_tar(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    build_dir = env.subst("$BUILD_DIR")
    
    bin_path = os.path.join(build_dir, f"{app_name}.bin")
    icon_path = os.path.join(project_dir, f"{app_name}_ICON.bin")
    tar_path = os.path.join(project_dir, f"{app_name}.tar")

    if not os.path.exists(bin_path):
        print("BIN not found, cannot create TAR")
        return

    cmd = ["tar", "-cf", tar_path]

    # Add BIN (relative via build dir)
    cmd += ["-C", build_dir, f"{app_name}.bin"]

    # Add ICON from project root
    if os.path.exists(icon_path):
        cmd += ["-C", project_dir, f"{app_name}_ICON.bin"]
    else:
        print("Warning: icon not found, creating TAR without icon")

    subprocess.run(cmd)
    print(f"Created TAR in project root: {tar_path}")

projenv.AddPostAction("$BUILD_DIR/firmware.bin", create_tar)

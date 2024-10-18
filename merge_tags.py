import subprocess
import sys

def merge_tag(repo_url, repo_name, subtree_path, tag):
    fetch_cmd = f"git fetch {repo_url} {tag}"
    merge_cmd = f"git merge -X subtree={subtree_path} FETCH_HEAD --log=99999999"

    try:
        subprocess.run(fetch_cmd, shell=True, check=True)
        print(f"Successfully fetched {repo_name} tag: {tag}")
        subprocess.run(merge_cmd, shell=True, check=True)
        print(f"Successfully merged {repo_name} tag: {tag}")
    except subprocess.CalledProcessError as e:
        print(f"Error fetching or merging {repo_name} tag: {tag}")
        print(e.output.decode())
        sys.exit(1)

# Repositories to merge
repos = [
    ("https://git.codelinaro.org/clo/la/kernel/msm-4.14", "msm-4.14", ""),
    ("https://git.codelinaro.org/clo/la/platform/vendor/qcom-opensource/wlan/qcacld-3.0", "qcacld-3.0", "drivers/staging/qcacld-3.0"),
    ("https://git.codelinaro.org/clo/la/platform/vendor/qcom-opensource/wlan/fw-api", "fw-api", "drivers/staging/fw-api"),
    ("https://git.codelinaro.org/clo/la/platform/vendor/qcom-opensource/wlan/qca-wifi-host-cmn", "qca-wifi-host-cmn", "drivers/staging/qca-wifi-host-cmn"),
    ("https://git.codelinaro.org/clo/la/platform/vendor/opensource/audio-kernel", "techpacka", "techpack/audio")
]

tag = input("Enter the tag to merge: ")

for repo_url, repo_name, subtree_path in repos:
    merge_tag(repo_url, repo_name, subtree_path, tag)

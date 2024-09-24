
condapath = "replace this text with your conda directory"
# Contains folders like "Scripts" and "shell", path does not end with / or \ (\\) 

version = "0.0.5"

import os
import re
import subprocess
import urllib.request as req
import traceback
import threading

IS_WINDOWS = os.name == "nt"

POWERSHELL = "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"
CMD = "C:\\Windows\\System32\\cmd.exe"

if IS_WINDOWS:
    def makeShortcut(filepath: str, target: str, args: str, iconpath: str, iconid: int):
        #$wsh = New-Object -comObject WScript.Shell
        #$sho_lowvram = $wsh.CreateShortcut("${base_path}\ComfyUI (Flux).lnk")
        #$sho_lowvram.TargetPath = "C:\Windows\System32\cmd.exe"
        #$sho_lowvram.Arguments = "/K `"${base_path}\${foldername}\${startfilename_lowvram}`""
        #$sho_lowvram.IconLocation = "shell32.dll,14"
        #$sho_lowvram.Save()
        return subprocess.call([POWERSHELL, 
                         f"$wsh = New-Object -comObject WScript.Shell; \
                         $sho_lowvram = $wsh.CreateShortcut(\"{filepath}\"); \
                         $sho_lowvram.TargetPath = \"{target}\"; \
                         $sho_lowvram.Arguments = \"{args}\"; \
                         $sho_lowvram.IconLocation = \"{iconpath},{iconid}\"; \
                         $sho_lowvram.Save();"])

def print_stdout(p):
    for line in iter(p.stdout.readline, b''):
        print(line.decode('utf-8'), end='')

def print_stderr(p):
    for line in iter(p.stderr.readline, b''):
        print(line.decode('utf-8'), end='')

#TODO better ctr+c/z/sigterm/sigkill handling?
class Conda:
    p = None
    print_thread = None
    print_err_thread = None
    def __init__(self, condapath: str):
        self.p = subprocess.Popen(args=[], executable=CMD,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd = os.getcwd(), shell = False)
        self.print_thread = threading.Thread(target=print_stdout, args=(self.p,))
        self.print_thread.start()
        self.print_err_thread = threading.Thread(target=print_stderr, args=(self.p,))
        self.print_err_thread.start()
        self.do(f"\"{condapath}\\Scripts\\activate.bat\"")

    def do(self, command: str):
        self.p.stdin.write((command + "\n").encode())
        self.p.stdin.flush()
        
    def end(self):
        self.p.stdin.close()
        self.p.wait()
        self.print_thread.join()
        self.print_err_thread.join()

def get_gpu() -> tuple[bool, str]:
    """Returns (is dedicated GPU, GPU name)"""
    dgpu = True
    gpu_name = ""
    if IS_WINDOWS:
        gpu_name = subprocess.check_output([POWERSHELL, "(Get-WmiObject Win32_VideoController).Name"])
        gpu_name = gpu_name.decode()
        ma = re.search(r"Intel\(R\) Arc\(TM\) ([A-C]\d{2,5}[A-Z]{0,2})", gpu_name)
        if ma:
            gpu_name = ma[0]
            dgpu = True
        else:
            gpu_name = "Unknown Possibly Integrated GPU"
            dgpu = False
    else:
        #TODO: Implement me
        gpu_name = "Not implemented"
    
    return (dgpu, gpu_name)

COLORS = {
    "DarkGreen": "\033[32m",
    "Cyan": "\033[36m",
    "White": "\033[37m",
    "Red": "\033[91m",
    "Green": "\033[92m",
    "Yellow": "\033[93m",
    "Default": "\033[0m"
}

def printColored(text, color: str, newline: bool = True):
    col = COLORS[color]
    end = "\n" if newline else ""
    print(col+str(text)+COLORS["Default"], end=end)

def readShortcut(path: str) -> str:
    #TODO: Implement me?
    #$conda_cmd_shortcut = $wsh.CreateShortcut("${Env:AppData}\Microsoft\Windows\Start Menu\Programs\Anaconda3 (64-bit)\Anaconda Prompt.lnk")
    #$conda_cmd_shortcut.Arguments | Select-String -Pattern "([A-Z]:[\w \\\/]+)\\Scripts\\activate\.bat"
    #$condapath = $conda_cmd_shortcut.Matches[0].Groups[1]
    return ""

def replaceTextInFile(filepath: str, orig: str, new: str):
    f = open(filepath, "r")
    file_string = ""
    for line in f:
        file_string += line
    f.close()
    loc = file_string.find(orig)
    if loc != -1:
        new_file_string = file_string[:loc] + new + file_string[loc+len(orig):]
        f = open(filepath, "w")
        f.write(new_file_string)
        f.close()

class PFCType:
    Key: str
    Name: str
    Description: str
    def __init__(self, guide: tuple[str, str] | tuple[str] | str):
        if type(guide) is tuple:
            self.Name = guide[0]
            self.Description = guide[len(guide) == 2]
        else:
            self.Name = guide
            self.Description = guide

        if type(guide) is tuple and len(guide) > 2:
            self.Key = guide[2]
        else:
            self.Key = self.Name[0].upper()

def inrange(val, min, max):
    """[min, max]"""
    return val >= min and val <= max

def promptForChoice(header: str, text: str, choices: list, default: int = 0, multiple: bool = False) -> int | tuple[int]:
    if (header != ""): printColored(header, "White")
    if (text != ""): print(text)

    choices_mod: list[PFCType] = []
    for c in choices:
        choices_mod.append(PFCType(c))

    while True:
        for i, choice in enumerate(choices_mod):
            color = "Yellow" if i == default else "White"
            printColored(f"[{choice.Key}] {choice.Name}  ", color=color, newline=False)
        printColored(f"[?] Help (default is \"{choices_mod[default].Key}\"): ", color="Default", newline=False)
        inp = input("")
        inp = inp.upper()
        if not multiple:
            for i, choice in enumerate(choices_mod):
                if (inp == choice.Key):
                    return i
        else:
            keys = list(filter(None, inp.upper().split(" ")))
            found = []
            for key in keys:
                for i, choice in enumerate(choices_mod):
                    if (key == choice.Key):
                        found.append(i)
                        break
            if len(found) == len(keys):
                return found
                    
        if (inp == "?"):
            for i, choice in enumerate(choices_mod):
                print(f"{choice.Key} - {choice.Description}")
    
    return -1

def formatTable(things: list[object] | list[dict], props: list[str], pad: int = 4, horizontalgap = True):
    if not (type(things[0]) is dict):
        get = getattr
    else:
        get = lambda thing, prop: thing[prop]
    
    longest_props = [0 for i in range(len(props))]
    for i, prop in enumerate(props):
        for thing in things:
            longest_props[i] = max(longest_props[i], len(str(get(thing, prop))))
        longest_props[i] = max(longest_props[i], len(prop))
    
    for i, prop in enumerate(props):
        print(prop.ljust(longest_props[i] + pad), end="")
    print("" if not horizontalgap else "\n")

    for thing in things:
        for i, prop in enumerate(props):
            attr = get(thing, prop)
            print(attr.ljust(longest_props[i] + pad), end="")
        print("")
    
def downloadFile(link: str, filename: str):
    opener = req.build_opener()
    opener.addheaders = [('User-agent', 'Mozilla/5.0')]
    req.install_opener(opener)
    req.urlretrieve(link, filename)

def clone_or_pull(link: str):
    folder = re.search(r"\/([^\/]+)$", link)[1]
    if not os.path.isdir(folder):
        subprocess.call(f"git clone {link}")
    else:
        subprocess.call("git pull", cwd=os.getcwd()+f"/{folder}")

class SkipErrorPrintException(Exception):
    pass

try:
    printColored(f"Script version: {version}", "DarkGreen")

    is_dgpu, gpu_name = get_gpu()
    base_path = os.getcwd()
    FOLDERNAME = "Comfy_Intel"
    CENVNAME = "cenv"
    
    UserProfile = os.environ.get("UserProfile", "")
    AppData = os.environ.get("AppData", "")
    Home = os.environ.get("HOME", "")

    # Autodetect conda
    if (not os.path.isdir(condapath)):
        conda_locs = [f"{UserProfile}\\miniconda3", f"{UserProfile}\\anaconda3", f"{Home}/anaconda3", f"{Home}/miniconda3"]

        if os.path.isdir(f"{AppData}\\Microsoft\\Windows\\Start Menu\\Programs\\Anaconda3 (64-bit)"):
            sh_args = readShortcut(f"{AppData}\\Microsoft\\Windows\\Start Menu\\Programs\\Anaconda3 (64-bit)\\Anaconda Prompt.lnk")
            condapath_re = re.search(r"([A-Z]:[\w \\\/]+)\\Scripts\\activate\.bat", sh_args)
            if condapath_re:
                conda_locs.append(condapath_re[1])
        
        for loc in conda_locs:
            if os.path.isdir(loc):
                condapath = loc
                break
    
    # Missing Conda/Git warnings 
    if (not os.path.isdir(condapath)):
        print("Conda not found. If you already have Conda installed, open this file with a text editor and put Conda's path in the \"quoted location\" on the first line.")
        print("You can download Conda from: https://docs.anaconda.com/miniconda/#latest-miniconda-installer-links")
        if IS_WINDOWS:
            print("Would you like to have this script install conda for you?")
            choice = promptForChoice("", "", (("Yes", "No")))
            if choice == 0:
                print("Downloading...")
                subprocess.call([CMD, "/C", "curl https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe -o miniconda.exe"])
                print("Installing...")
                subprocess.call([CMD, "/C", "start /wait .\\miniconda.exe /S"])
                print("Cleaning up...")
                subprocess.call([CMD, "/C", "del miniconda.exe"])
                print("Miniconda installed. Please run the script again.")
        raise SkipErrorPrintException
    
    
    try:
        o = subprocess.check_output(("git", "--version"))
    except:
        print("Git not found.")
        print("You can download Git from: https://git-scm.com/download/win")
        raise SkipErrorPrintException

    choices = (
        ("Set up ComfyUI", "Download ComfyUI and install dependencies and other things needed to run on Intel Arc"),
        ("Download a model")
    )

    chosen_install = promptForChoice("---", "What to do?", choices, 0)
    #chosen_install = 0

    if (chosen_install == 0):
        ####################################
        #         Install ComfyUI          #
        ####################################

        #if (os.path.isdir(FOLDERNAME)):
        #    print(f"A folder {FOLDERNAME} already exists.")
        #    print("Please delete it.")
        #    raise SkipErrorPrintException

        if is_dgpu:
            gpu_text = ["a", "discrete", gpu_name]
        else:
            gpu_text = ["an", "integrated", gpu_name]
        # Description of what is to be installed
        print("")
        printColored("A folder ", "Default", False)
        printColored(FOLDERNAME, "Cyan", False)

        if not os.path.isdir(FOLDERNAME):
            printColored(f" containing ComfyUI and Conda environment \"{CENVNAME}\" will be created in ", "Default", False)
            printColored(base_path, "Cyan", False)
        else:
            conda_text = "updated" if os.path.isdir(f"./{FOLDERNAME}/{CENVNAME}") else "created"
            printColored(f" exists, the contained ComfyUI will be updated and Conda environment \"{CENVNAME}\" {conda_text}", "Default", False)

        printColored(f", \ninstalling IPEX for {gpu_text[0]} ", "Default", False)
        printColored(gpu_text[1], "Cyan", False)
        printColored(f" {gpu_text[2]} and using Conda at ", "Default", False)
        printColored(os.path.join(condapath, ''), "Cyan", False)
        print(",\nas well as containing 1 batch file - used to launch Comfy (with --lowvram),")
        print("and a shortcut to it outside the folder.")
        print("\nContinue?")
        c = promptForChoice("", "", ("Yes", "No"))
        if(c):
            exit()
    
        class custom_node:
            Name: str
            Description: str
            link: str
            def __init__(self, Name, Description, link):
                self.Name = Name
                self.Description = Description
                self.link = link

        custom_nodes_info = (
            custom_node(Name="GGUF",            Description = "Flux.1 quantized below 8 bit, for Arc GPUs with <16GB of VRAM",  link="https://github.com/city96/ComfyUI-GGUF"),
            custom_node(Name="BrushNet",        Description = "More intelligent inpainting, and using any SD1.5/XL model",      link="https://github.com/nullquant/ComfyUI-BrushNet"),
            #custom_node(Name="Impact Pack",     Description = "Pack of nodes for object segmentation and dealing with masks",   link="https://github.com/ltdrdata/ComfyUI-Impact-Pack"),
            custom_node(Name="SUPIR",           Description = "High quality upscaling for realistic images",                    link="https://github.com/kijai/ComfyUI-SUPIR"),
            custom_node(Name="KJNodes",         Description = "Various misc. nodes",                                            link="https://github.com/kijai/ComfyUI-KJNodes"),
            custom_node(Name="rghtree",         Description = "Optimized execution, progressbar and various misc. nodes",       link="https://github.com/rgthree/rgthree-comfy"),
            custom_node(Name="ExtraModels",     Description = "Allows running additional non-SD models (such as Pixart)",       link="https://github.com/city96/ComfyUI_ExtraModels"),
            custom_node(Name="IPAdapter Plus",  Description = "Image Prompts",                                                  link="https://github.com/cubiq/ComfyUI_IPAdapter_plus"),
            custom_node(Name="Controlnet aux",  Description = "Additional Controlnet preprocessors",                            link="https://github.com/Fannovel16/comfyui_controlnet_aux"),
            custom_node(Name="Tiled KSampler",  Description = "KSampler for very large images",                                 link="https://github.com/BlenderNeko/ComfyUI_TiledKSampler"),
            #custom_node(Name="Pysssss scripts", Description = "Play sound node and other misc. nodes abd UI additions",         link="https://github.com/pythongosssss/ComfyUI-Custom-Scripts"),
        )
        
        print("Would you like to install all of the following custom nodes:\n")
        formatTable(custom_nodes_info, ("Name", "Description"))
        print("\nNote: Some of these require additional models to function, which you can download using this script after installing.")
        chosen_custom_nodes = promptForChoice("", "", ("Yes", "No"), 0)
    
        # Slightly more organized stuff
        if not os.path.isdir(FOLDERNAME): os.mkdir(FOLDERNAME)
        os.chdir(FOLDERNAME)
        
        
        # ComfyUI, hijacks
        clone_or_pull("https://github.com/comfyanonymous/ComfyUI")
        os.chdir("./ComfyUI/comfy")
        clone_or_pull("https://github.com/comfyanonymous/ComfyUI")
        print("Applying Disty's hijacks (thanks!)")
        replaceTextInFile("model_management.py", "as ipex", "as ipex\n    from ipex_to_cuda import ipex_init\n    ipex_init()")
        os.chdir("../..")
        
        # Install dependencies
        conda = Conda(condapath)
        if not os.path.isdir(CENVNAME):
            conda.do(f"conda create -p ./{CENVNAME} python=3.10 -y")
        conda.do(f"conda activate ./{CENVNAME}")
        conda.do("conda install pkg-config libuv -y")
        conda.do("pip install -r ./ComfyUI/requirements.txt")

        if (chosen_custom_nodes == 0):
            os.chdir("./ComfyUI/custom_nodes")
            for cn in custom_nodes_info:
                folder = re.search(r"\/([^\/]+)$", cn.link)[1]

                clone_or_pull(cn.link)
                if (os.path.exists(f"./{folder}/requirements.txt")):
                    conda.do(f"pip install -r ./ComfyUI/custom_nodes/{folder}/requirements.txt")
                
            #TODO: Implement Impact Pack setup
            os.chdir("../..")

        if IS_WINDOWS:
            url = "xpu" if is_dgpu else "mtl"

            conda.do(f"python -m pip install torch==2.3.1+cxx11.abi torchvision==0.18.1+cxx11.abi torchaudio==2.3.1+cxx11.abi intel-extension-for-pytorch==2.3.110+xpu \
                    --extra-index-url https://pytorch-extension.intel.com/release-whl/stable/{url}/us/")

            conda.do("pip install dpcpp-cpp-rt==2024.2.1 mkl-dpcpp==2024.2.1 onednn==2024.2.1")
        else:
            conda.do("python -m pip install torch==2.3.1+cxx11.abi torchvision==0.18.1+cxx11.abi torchaudio==2.3.1+cxx11.abi intel-extension-for-pytorch==2.3.110+xpu \
                              oneccl_bind_pt==2.3.100+xpu --extra-index-url https://pytorch-extension.intel.com/release-whl/stable/xpu/us/")
        
        conda.do("pip install numpy==1.26.4")
        conda.do("pip install onnxruntime-openvino")
        
        
        # Create start script/s? Maybe 1 script + 1 shortcut only to not confuse people too much.
        START_FILENAME_LOWVRAM=f"start_lowvram"
        if IS_WINDOWS:
            start_lowvram_filename = START_FILENAME_LOWVRAM + ".bat"
            start_lowvram_content = f"call \"{condapath}\\Scripts\\activate.bat\" \ncd /D \"%~dp0\" \ncall conda activate ./{CENVNAME} \ncd ./ComfyUI \npython ./main.py --bf16-unet --disable-ipex-optimize --lowvram"
        else:
            start_lowvram_filename = START_FILENAME_LOWVRAM + ".sh"
            start_lowvram_content = f"#!/bin/bash\n:("
            #TODO: Implement me
        f = open(start_lowvram_filename, 'w')
        f.write(start_lowvram_content)
        f.close()
        
        
        # Shortcut/s?
        if IS_WINDOWS:
            retc = makeShortcut(f"{base_path}\\ComfyUI.lnk", CMD, f"/K `\"{base_path}\\{FOLDERNAME}\\{start_lowvram_filename}`\"", "shell32.dll", 14)
            if retc != 0:
                print("An error ocurred when creating shortcut.")
                raise SkipErrorPrintException
        else:
            #TODO: Implement me
            print(".desktop file code here")

        conda.end()
        print("", flush=True)

        if (chosen_custom_nodes == 0):
            print("Applying SUPIR fixes...")
            replaceTextInFile("./cenv/lib/site-packages/open_clip/transformer.py", "x.to(torch.float32)", "x.to(self.weight.dtype)")
            replaceTextInFile("./ComfyUI/custom_nodes/ComfyUI-SUPIR/sgm/modules/diffusionmodules/sampling.py", "mps(device):", "mps(device) or comfy.model_management.is_intel_xpu():")
            print("Done.")


        printColored("\n\nComfyUI is set up. Press enter to continue.\n", "Green")
    elif(chosen_install == 1):
        ##################################
        #        Download a model        #
        ##################################

        if (not os.path.isdir(FOLDERNAME)):
            print("Please install ComfyUI first.")
            raise SkipErrorPrintException

        class DownloadableFile:
            link: str
            size: int
            """In MB"""
            dir: str
            filename_override: int
            def __init__(self, link, size, dir = "checkpoints", filename_override = ""):
                self.link=link
                self.size=size
                self.dir = dir
                self.filename_override = filename_override

            def get_filename(self) -> str:
                if self.filename_override:
                    return self.filename_override
                else:
                    m = re.search(r"\/([^\/]+)$", model.link)
                    if (m):
                        return m[1]
                    else:
                        raise f"Could not find filename for {model.link} ({model})"
                return ""

        class DownloadableCollection:
            models: list[DownloadableFile]
            name: str
            license: str | None
            def __init__(self, models, name, license=None):
                self.models = models
                self.name = name
                self.license = license

        t5_8 =              DownloadableFile("https://huggingface.co/comfyanonymous/flux_text_encoders/resolve/main/t5xxl_fp8_e4m3fn.safetensors", 4779, "clip")
        clip_l =            DownloadableFile("https://huggingface.co/comfyanonymous/flux_text_encoders/resolve/main/clip_l.safetensors", 346, "clip")
        ae =                DownloadableFile("https://huggingface.co/black-forest-labs/FLUX.1-schnell/resolve/main/ae.safetensors", 327, "vae")
        sdxl_vae_10 =       DownloadableFile("https://huggingface.co/stabilityai/sdxl-vae/resolve/main/sdxl_vae.safetensors", 359, "vae")

        clip_tune_text =    DownloadableFile("https://huggingface.co/zer0int/CLIP-GmP-ViT-L-14/resolve/main/ViT-L-14-TEXT-detail-improved-hiT-GmP-TE-only-HF.safetensors", 346)
        clip_tune_smooth =  DownloadableFile("https://huggingface.co/zer0int/CLIP-GmP-ViT-L-14/resolve/main/ViT-L-14-BEST-smooth-GmP-TE-only-HF-format.safetensors", 346)
        
        fluxdev8b_u =       DownloadableFile("https://huggingface.co/Kijai/flux-fp8/resolve/main/flux1-dev-fp8-e4m3fn.safetensors", 11621, "unet")
        fluxschnell8b_u =   DownloadableFile("https://huggingface.co/Kijai/flux-fp8/resolve/main/flux1-schnell-fp8-e4m3fn.safetensors", 11621, "unet")
        fluxdev4b_u =       DownloadableFile("https://huggingface.co/city96/FLUX.1-dev-gguf/resolve/main/flux1-dev-Q4_0.gguf", 6632, "unet")
        fluxschnell4b_u =   DownloadableFile("https://huggingface.co/city96/FLUX.1-schnell-gguf/resolve/main/flux1-schnell-Q4_0.gguf", 6632, "unet")

        sdxl_10_sft =       DownloadableFile("https://huggingface.co/stabilityai/stable-diffusion-xl-base-1.0/resolve/main/sd_xl_base_1.0.safetensors", 6775)
        jugxl_v9_rdp2_sft = DownloadableFile("https://huggingface.co/RunDiffusion/Juggernaut-XL-v9/resolve/main/Juggernaut-XL_v9_RunDiffusionPhoto_v2.safetensors", 6775)
        jugxl_rdp2li_sft =  DownloadableFile("https://huggingface.co/RunDiffusion/Juggernaut-XL-Lightning/resolve/main/Juggernaut_RunDiffusionPhoto2_Lightning_4Steps.safetensors", 7634)
        jugxl_v2_sft =      DownloadableFile("https://huggingface.co/RunDiffusion/Juggernaut-XL/resolve/main/juggernautXL_version2.safetensors", 6775)
        animagine_31_sft =  DownloadableFile("https://huggingface.co/cagliostrolab/animagine-xl-3.1/resolve/main/animagine-xl-3.1.safetensors", 6775)

        supir_f =           DownloadableFile("https://huggingface.co/Kijai/SUPIR_pruned/resolve/main/SUPIR-v0F_fp16.safetensors", 2856)
        supir_q =           DownloadableFile("https://huggingface.co/Kijai/SUPIR_pruned/resolve/main/SUPIR-v0Q_fp16.safetensors", 2856)

        powerpaint_1 =      DownloadableFile("https://huggingface.co/JunhaoZhuang/PowerPaint-v2-1/resolve/main/PowerPaint_Brushnet/diffusion_pytorch_model.safetensors", 3801, "inpaint/powerpaint", "powerpaint_v2.safetensors")
        powerpaint_2 =      DownloadableFile("https://huggingface.co/JunhaoZhuang/PowerPaint-v2-1/resolve/main/PowerPaint_Brushnet/pytorch_model.bin", 528, "inpaint/powerpaint")

        brushnet_15_seg =   DownloadableFile("https://drive.google.com/file/d/1uLQ55rdcFljdP_9iYGX5ZmJWzXFJast7/view?usp=drive_link", 2310, "inpaint/brushnet", "segmentation_mask.safetensors")
        brushnet_15_ran =   DownloadableFile("https://drive.google.com/file/d/1h_WOUK1SyV_YwoXAT7rRO012dI5sm3OE/view?usp=drive_link", 2310, "inpaint/brushnet", "random_mask.safetensors")

        brushnet_xl_seg =   DownloadableFile("https://drive.google.com/file/d/1yPhfQ3y60fXURr9_YfQrdE48y68IZjBN/view?usp=drive_link", 1390, "inpaint/brushnet_xl", "segmentation_mask.safetensors")
        brushnet_xl_ran =   DownloadableFile("https://drive.google.com/file/d/1e-vXMnlnEGmOr_s5P_hT0Uqp0nPuGFkT/view?usp=drive_link", 1390, "inpaint/brushnet_xl", "random_mask.safetensors")

        pixart_sigma_base = DownloadableFile("https://huggingface.co/PixArt-alpha/PixArt-Sigma/resolve/main/PixArt-Sigma-XL-2-1024-MS.pth", 2652)

        dreamshaper_8_ba =  DownloadableFile("https://civitai.com/api/download/models/128713?type=Model&format=SafeTensor&size=pruned&fp=fp16", 2008, filename_override="dreamshaper_8.safetensors")
        drmsh8r_8_inp_ba =  DownloadableFile("https://civitai.com/api/download/models/131004?type=Model&format=SafeTensor&size=pruned&fp=fp16", 2008, filename_override="dreamshaper_8_inpainting.safetensors")
        
        fluxdev4bit =       DownloadableCollection([ae, t5_8, clip_l, fluxdev4b_u], "Flux.1 Dev 4-bit", "https://huggingface.co/black-forest-labs/FLUX.1-dev/blob/main/LICENSE.md")
        fluxschnell4bit =   DownloadableCollection([ae, t5_8, clip_l, fluxschnell4b_u], "Flux.1 Schnell 4-bit")
        supir =             DownloadableCollection([supir_f, supir_q], "SUPIR")
        powerpaint =        DownloadableCollection([powerpaint_1, powerpaint_2], "PowerPaint (better Brushnet, SD1.5)")
        brushnet_15 =       DownloadableCollection([brushnet_15_ran, brushnet_15_seg], "Brushnet (SD1.5)")
        brushnet_xl =       DownloadableCollection([brushnet_xl_ran, brushnet_xl_seg], "Brushnet XL")
        sdxl_10 =           DownloadableCollection([sdxl_10_sft], "SDXL 1.0")
        jugxl_v9_rdp2 =     DownloadableCollection([jugxl_v9_rdp2_sft], "Juggernaut XL v9 RunDiffusion Photo v2")
        jugxl_v9_rdp2_li =  DownloadableCollection([jugxl_rdp2li_sft], "Juggernaut XL v9 RDP v2 Lightning 4-step") 
        jugxl_v2 =          DownloadableCollection([jugxl_v2_sft], "Juggernaut XL v2")
        animagine_31 =      DownloadableCollection([animagine_31_sft], "Animagine XL 3.1")
        fluxdev8bit =       DownloadableCollection([ae, t5_8, clip_l, fluxdev8b_u], "Flux.1 Dev 8-bit", "https://huggingface.co/black-forest-labs/FLUX.1-dev/blob/main/LICENSE.md")
        fluxschnell8bit =   DownloadableCollection([ae, t5_8, clip_l, fluxschnell8b_u], "Flux.1 Schnell 8-bit")
        pixart_sigma =      DownloadableCollection([sdxl_vae_10, t5_8, pixart_sigma_base], "Pixart Sigma")
        dreamshaper_8 =     DownloadableCollection([dreamshaper_8_ba], "Dreamshaper 8 (SD1.5)")
        dreamshaper_8_inp = DownloadableCollection([drmsh8r_8_inp_ba], "Dreamshaper 8 Inpainting (SD1.5)")
        clip_finetunes =    DownloadableCollection([clip_tune_smooth, clip_tune_text], "CLIP-L finetunes by zer0int")

        collections = [fluxdev4bit, fluxschnell4bit, supir, powerpaint, brushnet_15, brushnet_xl, sdxl_10, jugxl_v9_rdp2, jugxl_v9_rdp2_li, jugxl_v2, animagine_31, pixart_sigma, fluxdev8bit, fluxschnell8bit, dreamshaper_8, dreamshaper_8_inp, clip_finetunes]

        os.chdir(FOLDERNAME)

        print("Which model would you like to download?\n")

        table = []
        choices = []
        for i, coll in enumerate(collections):
            table_dict = {}
            table_dict["Name"] = (str(i+1) + ".").ljust(4) + coll.name
            size_part = 0
            size_base = 0
            for model in coll.models:
                if model in [ae, t5_8, clip_l]:
                    size_part += model.size
                else:
                    size_base += model.size
            size_str = str(size_base / 1000.0)[:4] + "GB"
            if size_part > 0:
                size_str += " + " + str(size_part / 1000.0)[:4] + "GB"
            table_dict["Size"] = size_str
            table.append(table_dict)
            choices.append((str(i+1), coll.name, str(i+1)))
        
        formatTable(table, ["Name", "Size"])

        print("\nAll Flux/Pixart models are downloaded piecemeal (separate VAE, FP8 T5, CLIP, UNET)")
        print("The VAE, T5 and CLIP can be used for all Flux models and account for 5.34GB.")
        print("The SDXL VAE and T5 can be used for Pixart Sigma.")
        print("They will not be redownloaded if you already have them, to save space.")

        print("\nType multiple numbers separated by spaces to download many models at once.")

        ids = promptForChoice("", "", choices, 0, multiple=True)
        ids = [int(id) for id in ids]
        os.chdir("./ComfyUI/models/")

        if len(ids) > 1:
            models: list[DownloadableFile] = []
            for id in ids:
                models.extend(collections[id].models)
            models = list(set(models))
            sum = 0
            for model in models: 
                if not os.path.exists(f"./{model.dir}/{model.get_filename()}"):
                    sum += model.size
            if sum < 1:
                print("All chosen models are already downloaded.")
                exit()
            print(f"About to download {str(sum / 1000.0)[:4]}GB, continue?")
            cont = promptForChoice("", "", (("Yes"), ("No")))
            if cont == 1:
                exit()

        # Don't randomly decide to ask for license agreeing mid-download because user decided to choose them out of order
        for id in ids:
            collection = collections[id]
            if (collection.license):
                choices = ("Agree", "Disagree")
                agree = promptForChoice(f"Please review and agree with the {collection.name} model's license, which can be found at:", collection.license + "\n(ctrl+left click a link to open in browser)", choices, 0)
                if(agree != 0):
                    print("License not accepted. Exiting...")
                    raise SkipErrorPrintException

        for id in ids:
            collection = collections[id]
            printColored(f"\nDownloading {collection.name}.", "White")
            for model in collection.models:
                filename = model.get_filename()
                os.makedirs(f"./{model.dir}", exist_ok=True)
                if (os.path.exists(f"./{model.dir}/{filename}")):
                    print(f"File {filename} ({model.link})\nalready exists...")
                else:
                    print(f"\nFile {filename}, {model.size}MB, link:\n{model.link}")
                    print("Downloading...", end="", flush=True)

                    os.chdir(f"./{model.dir}")
                    downloadFile(model.link, filename)
                    os.chdir("..")

        plural = "s" if len(ids) > 1 else ""
        printColored(f"\n\nFinished downloading model{plural}. Press enter to continue.\n", "Green")

except Exception as e:
    if (not(type(e) is SkipErrorPrintException)):
        printColored("An error occurred:", "Red")
        print(traceback.format_exc())
        #pdb.set_trace()
    print("\nPress enter to continue...")

input()

$condapath="replace this text with your conda directory"
# Contains folders like "Scripts" and "shell", path does not end with / or \ (\\) 

function MakeChoice{
    param($choice_text, $choice_help)
    if (![bool]$choice_help) {$choice_help=$choice_text}
    New-Object System.Management.Automation.Host.ChoiceDescription "&$choice_text", "$choice_help"
}

# Key is not shift, alt or ctrl
function ReadMagicKey{
    $k = $Host.UI.RawUI.ReadKey("IncludeKeyDown,NoEcho")
    while (@(16, 17, 18) -contains $k.VirtualKeyCode){
        $k = $Host.UI.RawUI.ReadKey("IncludeKeyDown,NoEcho")
    }
    $k
}

$skip_error_print = "12893ehn91bncd91"
try {


    $wsh = New-Object -comObject WScript.Shell
    $dgpu = (Get-WmiObject Win32_VideoController).Name | Select-String -Pattern "Intel\(R\) Arc\(TM\) ([A-C]\d{2,5}[A-Z]{0,2})"
    $base_path = $PWD[0]
    $foldername = "Comfy_Intel"
    
    # Autodetect conda
    if (-not (Test-Path $condapath)) {
        $condapath = "${Env:UserProfile}\miniconda3"
        if (-not (Test-Path $condapath)) {
            $condapath = "${Env:UserProfile}\anaconda3"
            if (-not (Test-Path $condapath)) {
                if (Test-Path "${Env:AppData}\Microsoft\Windows\Start Menu\Programs\Anaconda3 (64-bit)"){
                    try {
                        $conda_cmd_shortcut = $wsh.CreateShortcut("${Env:AppData}\Microsoft\Windows\Start Menu\Programs\Anaconda3 (64-bit)\Anaconda Prompt.lnk")
                        $conda_cmd_shortcut.Arguments | Select-String -Pattern "([A-Z]:[\w \\\/]+)\\Scripts\\activate\.bat"
                        $condapath = $conda_cmd_shortcut.Matches[0].Groups[1]
                    }
                    catch {
                        $condapath = "Not found"
                    }
                }else{
                    $condapath = "Not found"
                }
            }
        }
    }
    
    # Missing Conda/Git warnings 
    if (-not (Test-Path $condapath)) {
        Write-Output "Conda not found. If you already have Conda installed, open this file with a text editor and put Conda's path in the `"quoted location`" on the first line."
        Write-Output "You can download Conda from: https://docs.anaconda.com/miniconda/#latest-miniconda-installer-links"
        throw $skip_error_print
    }
    
    try {
        git --version | Out-Null
    } 
    catch {
        Write-Output "Git not found."
        Write-Output "You can download Git from: https://git-scm.com/download/win"
        throw $skip_error_print
    }

    $choices = [System.Management.Automation.Host.ChoiceDescription[]]@(
        (MakeChoice "Set up ComfyUI" "Download ComfyUI and install dependencies and other things needed to run on Intel Arc"),
        (MakeChoice "Download a model")
        (MakeChoice "Activate environment" "Convenience option to activate the conda environment (after ComfyUI is installed)")
    )
    $chosen = $Host.UI.PromptForChoice("---", "What to do?", $choices, 0)
    #$chosen = 0

    if ($chosen -eq 0){
        ####################################
        #         Install ComfyUI          #
        ####################################

        if (Test-Path $foldername) {
            Write-Output "A folder $foldername already exists."
            Write-Output "Please delete it."
            throw $skip_error_print
        }
    
        # User warning
        $gpu_text = if ($dgpu) {("a ", "discrete", " GPU $dgpu")} else {("an ", "integrated", " GPU")}
        Write-Host "A folder "                                -NoNewline
        Write-Host $foldername                                -NoNewline -ForegroundColor Cyan
        Write-Host " will be created in "                     -NoNewline
        Write-Host "$base_path\"                              -NoNewline -ForegroundColor Cyan
        Write-Host ", `ninstalling IPEX for $($gpu_text[0])"  -NoNewline 
        Write-Host $gpu_text[1]                               -NoNewline -ForegroundColor Cyan 
        Write-Host "$($gpu_text[2]) and using Conda at "      -NoNewline
        Write-Host "$condapath\"                              -NoNewline -ForegroundColor Cyan
        Write-Output ",`nas well as containing 1 batch file - used to launch Comfy (with --lowvram),"
        Write-Output "and a shortcut to it outside the folder."
        Write-Output "`nContinue? y/n:"
        $continue = ReadMagicKey
        if ($continue.Character -ne "y") {
            Exit
        }
        Write-Host "`n"
    
    
        # Slightly more organized stuff
        mkdir $foldername | Out-Null
        Set-Location ./${foldername} | Out-Null
        
        
        # Set up code
        git clone https://github.com/comfyanonymous/ComfyUI
        
        Set-Location ./ComfyUI/comfy
        git clone https://github.com/Disty0/ipex_to_cuda
        Write-Output "Applying Disty's hijacks (thanks!)"
        (Get-Content model_management.py).Replace("as ipex", "as ipex`n    from ipex_to_cuda import ipex_init`n    ipex_init()") | `
        Set-Content model_management.py
        Set-Location ../..
        
        Set-Location ./ComfyUI/custom_nodes
        git clone https://github.com/city96/ComfyUI-GGUF
        Set-Location ../..
        
        # Install dependencies
        & ${condapath}/shell/condabin/conda-hook.ps1
        conda create -p ./cenv python=3.10 -y
        conda activate ./cenv
        conda install pkg-config libuv libpng -y
        pip install -r ./ComfyUI/requirements.txt
        pip install -r ./ComfyUI/custom_nodes/ComfyUI-GGUF/requirements.txt
        if ($dgpu) {
            python -m pip install torch==2.1.0.post3 torchvision==0.16.0.post3 torchaudio==2.1.0.post3 intel-extension-for-pytorch==2.1.40+xpu `
            --extra-index-url https://pytorch-extension.intel.com/release-whl/stable/xpu/us/
        } else {
            python -m pip install torch==2.1.0.post3 torchvision==0.16.0.post3 torchaudio==2.1.0.post3 intel-extension-for-pytorch==2.1.40+xpu `
            --extra-index-url https://pytorch-extension.intel.com/release-whl/stable/mtl/us/
        }
        pip install dpcpp-cpp-rt==2024.2.1 mkl-dpcpp==2024.2.1 onednn==2024.2.1
        pip install numpy==1.26.4
        pip install onnxruntime-openvino
        
        
        # Create start script/s? Maybe 1 script + 1 shortcut only to not confuse people too much.
        $startfilename_lowvram="start_lowvram.bat"
        Write-Output "call `"${condapath}\Scripts\activate.bat`" `ncd /D `"%~dp0`" `ncall conda activate ./cenv `ncd ./ComfyUI `npython ./main.py --bf16-unet --disable-ipex-optimize --lowvram" | `
        Set-Content "${base_path}/${foldername}/${startfilename_lowvram}"
        
        
        # Shortcut/s?
        $sho_lowvram = $wsh.CreateShortcut("${base_path}\ComfyUI (Flux).lnk")
        $sho_lowvram.TargetPath = "C:\Windows\System32\cmd.exe"
        $sho_lowvram.Arguments = "/K `"${base_path}\${foldername}\${startfilename_lowvram}`""
        $sho_lowvram.IconLocation = "shell32.dll,14"
        $sho_lowvram.Save()
        
        conda deactivate
        Set-Location ..
        Write-Host "`n`nComfyUI is set up. Press any key to continue.`n" -ForegroundColor Green
    }elseif($chosen -eq 1){
        ##################################
        #        Download a model        #
        ##################################

        if (!(Test-Path $foldername)) {
            Write-Output "Please install ComfyUI first."
            throw $skip_error_print
        }

        $t5_8 = [PSCustomObject]@{link="https://huggingface.co/comfyanonymous/flux_text_encoders/resolve/main/t5xxl_fp8_e4m3fn.safetensors"; size=4779 }
        $clip_l = [PSCustomObject]@{link="https://huggingface.co/comfyanonymous/flux_text_encoders/resolve/main/clip_l.safetensors"; size=240 }
        $ae = [PSCustomObject]@{link="https://huggingface.co/black-forest-labs/FLUX.1-schnell/resolve/main/ae.safetensors"; size=327 }
        
        $fluxdev8b_u = [PSCustomObject]@{link="https://huggingface.co/Kijai/flux-fp8/resolve/main/flux1-dev-fp8-e4m3fn.safetensors"; size=11621 }
        $fluxschnell8b_u = [PSCustomObject]@{link="https://huggingface.co/Kijai/flux-fp8/resolve/main/flux1-schnell-fp8-e4m3fn.safetensors"; size=11621 }
        $fluxdev4b_u = [PSCustomObject]@{link="https://huggingface.co/city96/FLUX.1-dev-gguf/resolve/main/flux1-dev-Q4_0.gguf"; size=6632 }
        $fluxschnell4b_u = [PSCustomObject]@{link="https://huggingface.co/city96/FLUX.1-schnell-gguf/resolve/main/flux1-schnell-Q4_0.gguf"; size=6632 }

        $sdxl_10_sft = [PSCustomObject]@{link="https://huggingface.co/stabilityai/stable-diffusion-xl-base-1.0/resolve/main/sd_xl_base_1.0.safetensors"; size=6775 }
        $jugxl_v2_sft = [PSCustomObject]@{link="https://huggingface.co/RunDiffusion/Juggernaut-XL/resolve/main/juggernautXL_version2.safetensors"; size=6775 }
        $animagine_31_sft = [PSCustomObject]@{link="https://huggingface.co/cagliostrolab/animagine-xl-3.1/resolve/main/animagine-xl-3.1.safetensors"; size=6775 }

        $folder_mappings = @{"vae"="vae"; "clip"="clip"; "t5"="clip"; "unet"="unet"; "checkpoint"="checkpoints";}

        $fluxdev8bit = [PSCustomObject]@{ vae=$ae; t5=$t5_8; clip=$clip_l; unet=$fluxdev8b_u; name="Flux.1 Dev 8-bit"; license="https://huggingface.co/black-forest-labs/FLUX.1-dev/blob/main/LICENSE.md" }
        $fluxschnell8bit = [PSCustomObject]@{ vae=$ae; t5=$t5_8; clip=$clip_l; unet=$fluxschnell8b_u; name="Flux.1 Schnell 8-bit" }
        $fluxdev4bit = [PSCustomObject]@{ vae=$ae; t5=$t5_8; clip=$clip_l; unet=$fluxdev4b_u; name="Flux.1 Dev 4-bit"; license="https://huggingface.co/black-forest-labs/FLUX.1-dev/blob/main/LICENSE.md" }
        $fluxschnell4bit = [PSCustomObject]@{ vae=$ae; t5=$t5_8; clip=$clip_l; unet=$fluxschnell4b_u; name="Flux.1 Schnell 4-bit" }
        $sdxl_10 = [PSCustomObject]@{ checkpoint=$sdxl_10_sft; name="SDLX 1.0" }
        $jugxl_v2 = [PSCustomObject]@{ checkpoint=$jugxl_v2_sft; name="Juggernaut XL v2" }
        $animagine_31 = [PSCustomObject]@{ checkpoint=$animagine_31_sft; name="AnimagineXL 3.1" }

        $models = $fluxdev8bit, $fluxschnell8bit, $fluxdev4bit, $fluxschnell4bit, $sdxl_10, $jugxl_v2, $animagine_31

        Set-Location ./$foldername
        #& ${condapath}/shell/condabin/conda-hook.ps1
        #conda activate ./cenv
        #if (!((pip list) -match "huggingface_hub")){ # This is probably not actually faster than just pip installing every time
            #pip install huggingface_hub
            #Write-Output "Would install hfhub"
        #}

        #Write-Output "NOTE: You will need to log in to huggingface in order to download Flux or SDXL. Their licenses still apply to `n"

        # TODO?: More programmatic table?
        Write-Output "Which model would you like to download?"
        Write-Output "1. 8-bit Flux.1-Dev        11.9GB + 5.47GB"
        Write-Output "2. 8-bit Flux.1-Schnell    11.9GB + 5.47GB"
        Write-Output "3. 4-bit Flux.1-Dev        6.79GB + 5.47GB"
        Write-Output "4. 4-bit Flux.1-Schnell    6.77GB + 5.47GB"
        Write-Output "5. SDXL 1.0                6.94GB"
        Write-Output "6. Juggernaut XL v2        6.94GB"
        Write-Output "7. Animagine XL 3.1        6.94GB"
        Write-Output ""
        Write-Output "All Flux models are downloaded piecemeal (separate VAE, FP8 T5, CLIP, UNET)"
        Write-Output "The VAE, T5 and CLIP can be used for all Flux models and account for 5.47GB."
        Write-Output "They will not be redownloaded if you already have them, to save space."
        
        #? Should the help just be all links?
        $choices = [System.Management.Automation.Host.ChoiceDescription[]]@(
            (MakeChoice "1" "8-bit (fp8_e4m3fn) quantized version of Flux.1-Dev (20-50 steps)           https://huggingface.co/black-forest-labs/FLUX.1-dev"),
            (MakeChoice "2" "8-bit (fp8_e4m3fn) quantized version of Flux.1-Schnell (fast, 2-8 steps)   https://huggingface.co/black-forest-labs/FLUX.1-schnell"),
            (MakeChoice "3" "4-bit (Q4_0) quantized version of Flux.1-Dev (20-50 steps)                 https://huggingface.co/black-forest-labs/FLUX.1-dev"),
            (MakeChoice "4" "4-bit (Q4_0) quantized version of Flux.1-Schnell (fast, 2-8 steps)         https://huggingface.co/black-forest-labs/FLUX.1-schnell"),
            (MakeChoice "5" "Stable Diffusion XL base 1.0   https://huggingface.co/stabilityai/stable-diffusion-xl-base-1.0"),
            (MakeChoice "6" "Juggernaut XL v2               https://huggingface.co/RunDiffusion/Juggernaut-XL"),
            (MakeChoice "7" "Animagine XL 3.1               https://huggingface.co/cagliostrolab/animagine-xl-3.1")
        )

        $id = $Host.UI.PromptForChoice("", "", $choices, 0)
        Set-Location ./ComfyUI/models/
        $model = $models[$id]

        if ($model.license){
            $choices = [System.Management.Automation.Host.ChoiceDescription[]]@((MakeChoice "Agree"), (MakeChoice "Disagree"))
            $agree = $Host.UI.PromptForChoice("Please review and agree with the model's license, which can be found at:", $model.license + "`n(ctrl+left click a link to open in browser)", $choices, 0)
            if($agree -ne 0){
                Write-Output "License not accepted. Exiting..."
                conda deactivate
                Set-Location ../../..
                throw $skip_error_print
            }
        }

        foreach ($key in $folder_mappings.keys) {
            if ($model.$key) {
                if (($model.$key.link) -match '\/([^\/]+)$') {
                    $filename = $Matches[1]
                } else {
                    throw "Could not find filename for $($model.$key.link) ($model, $key)"
                }

                if (Test-Path "./$($folder_mappings[$key])/$filename") {
                    Write-Output "File $filename ($key) already exists..."
                }else{
                    Write-Output "`nWill download $key, filename $filename, $($model.$key.size)MB, link:"
                    Write-Output $model.$key.link
                    Write-Host "Downloading..." -NoNewline
    
                    Set-Location "./$($folder_mappings[$key])"
                    $ProgressPreference = "SilentlyContinue" # Progressbar massively slows downloads, 10x or more slowdown
                    Invoke-WebRequest $model.$key.link -OutFile $filename # wget equivalent

                    Set-Location ..
                }
            }
        }
        Set-Location ../../..
        conda deactivate
        Write-Host "`n`nFinished downloading model. Press any key to continue.`n" -ForegroundColor Green

    }else{
        ###################
        #  Activate cenv  #
        ###################

        if (!(Test-Path $foldername)) {
            Write-Output "Please install ComfyUI first."
            throw $skip_error_print
        }
        & ${condapath}/shell/condabin/conda-hook.ps1
        conda activate "./$foldername/cenv"
        Write-Host "Conda environment activated." -NoNewline
        throw $skip_error_print
    }

}
catch {
    if (-not $_.FullyQualifiedErrorId.Equals($skip_error_print)) {
        Write-Host "An error occurred:" -ForegroundColor Red
        Write-Output "$_"
    }
    Write-Host "`nPress any key to continue..."
}

$null = ReadMagicKey
#ifdef _WIN32
#include <process.h>
#include <process.h>
#define execv _execv
#define execl _execl
    #include <process.h>
#define execv _execv
#define execl _execl
    #define popen _popen
    #define spawnv _spawnv
    #define spawnvp _spawnvp
#else
    #include <unistd.h>
    // TODO: Wrap posix_spawn and posix_spawnp into spawnv and spawnvp
#endif
#include <stdio.h>
#include <vector>
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <fstream>
// #include <thread> // Dunno how to fork() on windows
#include <filesystem>
#include <cstdlib> //getenv(), system()

#define max(a, b) a>b ? a : b

using std::string;
using std::to_string;
using std::regex;
using std::pair;
using std::vector;
using namespace std::string_literals; // For std::string literals, "blabla"s -> string (not char*) literal
using std::regex_search;
using std::smatch;
using std::cout;
using std::cin;
using std::getline;
using std::endl;
using std::ifstream;
using std::ofstream;

// using std::thread;

using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::create_directory;
using std::filesystem::create_directories;
using std::filesystem::current_path;
using std::filesystem::path;

const string POWERSHELL = "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
const string CMD = "C:\\Windows\\System32\\cmd.exe";
const string VERSION = "0.0.6";

/// @brief Converts a std::vector<string> into a char* vector (char**), with convenient construction and destruction. Access using member v.
struct convertVecStr{
    char** v;
    convertVecStr(const vector<string>& _v){
        v = new char*[_v.size()+1];
        for(int i=0;i<_v.size();i++){
            v[i] = new char[_v[i].length()];
            strcpy(v[i], _v[i].c_str());
        }
        v[_v.size()] = nullptr;
    }
    ~convertVecStr(){
        char* subv;
        for(int i=0; v[i]; i++){
            delete[] v[i];
        }
        delete[] v;
    }

};

template<typename t>
int find(const vector<t>& v, const t& elem){
    for(int i=0; i<v.size(); i++){
        const t &el2 = v[i]; // Helps with error readability
        if(el2 == elem){
            return i;
        }
    }
    return -1;
}

int executeCommand(const vector<string> &command){//, bool blocking=true){
    // system() implementation, doesn't allow setting argv
    //string folded;
    //for(int i=0; i<command.size(); i++){
    //    folded += command[i] + " "s;
    //}
    //return system(folded.c_str());

    convertVecStr converted(command);
    return spawnvp(_P_WAIT, converted.v[0], converted.v);
}

/// Always blocking
string executeCommandReadOutput(const vector<string> &command){
    string res;
    string folded_command = "";
    for(auto s : command){
        folded_command = folded_command + s + " "s;
    }
    auto pipe = popen(folded_command.c_str(), "r");
    char* buf = new char[256];
    while (fgets(buf, 256, pipe)) {
        res += buf;
    }
    delete[] buf;
    _pclose(pipe);
    return res;
}

int makeShortcut(const string &filepath, const string &target, const string &args, const string &iconpath = "shell32.dll"s, int iconid = 14){
  //$wsh = New-Object -comObject WScript.Shell
  //$sho_lowvram = $wsh.CreateShortcut("${base_path}\ComfyUI (Flux).lnk")
  //$sho_lowvram.TargetPath = "C:\Windows\System32\cmd.exe"
  //$sho_lowvram.Arguments = "/K `"${base_path}\${foldername}\${startfilename_lowvram}`""
  //$sho_lowvram.IconLocation = "shell32.dll,14"
  //$sho_lowvram.Save()

  string command = "\"$wsh = New-Object -comObject WScript.Shell; "s +
  "$sho_lowvram = $wsh.CreateShortcut(\""s + filepath + "\"); "s +
  "$sho_lowvram.TargetPath = \'"s + target + "\'; "s +
  "$sho_lowvram.Arguments = \'"s + args + "\';\n"s +
  "$sho_lowvram.IconLocation = \'"s + iconpath + ","s + to_string(iconid) + "\'; "s +
  "$sho_lowvram.Save();\""s;

  return executeCommand({POWERSHELL, "-noprofile", command});
}

string readShortcut(const string &filepath) {
    //$wsh = New-Object -comObject WScript.Shell
    //$conda_cmd_shortcut = $wsh.CreateShortcut("${Env:ProgramData}\Microsoft\Windows\Start Menu\Programs\Anaconda3 (64-bit)\Anaconda Prompt.lnk")
    //$conda_cmd_shortcut.Arguments

    string command = "\"$wsh = New-Object -comObject WScript.Shell; "s +
    "$sho = $wsh.CreateShortcut(\'" + filepath + "\'); "s +
    "Write-Host $sho.Arguments\""s; // Command must be in quotations with no newlines
    
    return executeCommandReadOutput({POWERSHELL, "-noprofile", command});
}

class Conda{
    vector<string> commands;
public:
    Conda(const string &condapath) {
      //buf += "(& \""s + condapath + "\\Scripts\\conda.exe\" \"shell.powershell\" \"hook\") | Out-String | ?{$_} | Invoke-Expression\n";
      commands = {CMD, "/C", condapath + "\\Scripts\\activate.bat"s};
      return;
    }
    void command(const string &text) {
        commands.push_back(text);
    }
    void end() {
        cout << "Executing:\n";
        for(int i=0; i<commands.size(); i++){
            cout << commands[i] << " | ";
        }
        executeCommand(commands);
    }
};

pair<bool, string> get_gpu() {
    bool dgpu = true;
    string gpu_name = "";
    #ifdef _WIN32
    string out = executeCommandReadOutput({POWERSHELL, "-noprofile", "(Get-WmiObject Win32_VideoController).Name"});
    auto r = regex("Intel\\(R\\) Arc\\(TM\\) ([A-C]\\d{2,5}[A-Z]{0,2})");
    smatch sm;
    if(regex_search(out, sm, r)){
        gpu_name = sm[0];
        dgpu = true;
    }else{
        gpu_name = "Unknown Possibly Integrated GPU";
        dgpu = false;
    }
    #else
    #endif

    return {dgpu, gpu_name};
}

namespace COLORS{
    const string DarkGreen = "\033[32m"s;
    const string Cyan = "\033[36m"s;
    const string White = "\033[37m"s;
    const string Red = "\033[91m"s;
    const string Green = "\033[92m"s;
    const string Yellow = "\033[93m"s;
    const string Default = "\033[0m"s;
}

const vector<string> COLORS_VEC = {COLORS::DarkGreen, COLORS::Cyan, COLORS::White, COLORS::Red, COLORS::Green, COLORS::Yellow, COLORS::Default};

void printColored(const string &text, const string &color, bool newline = true){
    _ASSERT(find(COLORS_VEC, color) != -1);
    cout << color << text << COLORS::Default;
    if(newline){
        cout << endl;
    }
}

void print(const string &text, const string &end="\n", bool flush=false){
    cout << text << end;
    if(flush){
        cout << std::flush;
    }
}

bool replaceTextInFile(const string &filepath, const string &orig, const string &newtext){
    _ASSERT(newtext.find(orig) == string::npos);

    ifstream f_in(filepath);
    string text = "", buf = "";
    while(!f_in.eof()){
        getline(f_in, buf);
        text += buf;
    }
    f_in.close();
    int found = text.find(orig);
    if(found != string::npos){
        text.replace(found, orig.length(), newtext);
    } else{
        return false;
    }
    ofstream f_out(filepath);
    f_out << newtext;
    f_out.close();
    return true;
}


string upper(const string& s){
    string res;
    // Kinda dumb?
    for (auto c : s){
        if(c >= 'a' && c <= 'z'){
            c += 'A' - 'a';
        }
        res += c;
    }
    return res;
}

struct PFCType{
    string Key;
    string Name;
    string Description;
    PFCType(const string &_s){
        Name = _s;
        Description = _s;
        Key = _s[0];
    }
    PFCType(const string &_Name, const string &_Description){
        Name = _Name;
        Description = _Description;
        Key = Name[0];
    }
    PFCType(const string &_Name, const string &_Description, const string &_Key){
        Name = _Name;
        Description = _Description;
        Key = _Key;
    }
    PFCType(const vector<string> &v){
        if(v.size() == 1){
            Name = Description = v[0];
            Key = v[0][0];
        }else{
            Name = v[0];
            Description = v[1];
            if(v.size() == 3){
                Key = v[2];
            }else{
                Key = Name[0];
            }
        }
    }
};

template<typename t>
bool inrange(t val, t min, t max) {
    return t >= min && t <= max;
}

string input(const string &text=""s){
    cout << text;
    string res;
    getline(cin, res);
    return res;
}

vector<int> promptForChoice(const string &header, const string &text, const vector<PFCType> &choices, int def=0, bool multiple=false){
    if (header != "") printColored(header, COLORS::White);
    if (text != "") print(text);

    while(true){
        for(int i=0; i<choices.size(); i++){
            auto& choice = choices[i];
            string color = i == def ? COLORS::Yellow : COLORS :: White;
            printColored("["s + choice.Key + "] "s + choice.Name + "  ", color, false);
        }
        printColored("[?] Help (default is \""s + choices[def].Key + "\"): "s, COLORS::Default, false);
        string inp = input("");
        inp = upper(inp);
        if (inp == "?"){
            for(int i=0; i<choices.size(); i++){
                print(choices[i].Name + " - "s + choices[i].Description);
            }
        }
        if (!multiple){
            for (int i=0; i<choices.size(); i++){
                if (inp == choices[i].Key){
                    return {i};
                }
            }
        }else{
            vector<string> keys;
            bool newkey = false;
            keys.push_back("");
            for(int i=0; i<inp.length(); i++){
                if(newkey && inp[i] != ' '){
                    newkey = false;
                    keys.push_back("");
                }
                if(inp[i] == ' '){
                    newkey = true;
                }
                if(!newkey){
                    keys[keys.size()-1] += inp[i];
                }
            }
            vector<int> found;
            for(int i=0; i<keys.size(); i++){
                for(int j=0; j<choices.size(); j++){
                    if(choices[j].Key == keys[i]) {
                        found.push_back(j);
                    }
                }
            }
            if (found.size() == keys.size()){
                return found;
            }
        }
    }
    return {-1};
}

/// @brief Pads a string to fit a certain size.
/// @param s String to be padded.
/// @param pad_count Target size to fit.
/// @param pad_char Char to pad with.
/// @param pad_left 0 = pad on right (   str), 1 = pad on left (str   )
/// @return Padded string.
string pad(string s, int pad_count, char pad_char=' ', bool pad_left=true){
    int paddedlen = pad_count - s.size();
    if (paddedlen <= 0){
        return s;
    }
    string padding(paddedlen, pad_char);
    if (pad_left){
        return s + padding;
    }else{
        return padding + s;
    }
}

void formatTable(const vector<vector<string> > &things, const vector<string> &names, int extra_pad=4, bool horizontalGap = true){
    _ASSERT(things[0].size() == names.size());

    vector<int> longest_props(names.size(), 0);
    for(int i=0;i<things.size();i++){
        for(int j=0; j<things[i].size(); j++){
            longest_props[i] = max(longest_props[j], things[i][j].length());
        }
    }
    for(int i=0; i<names.size(); i++){
        longest_props[i] = max(longest_props[i], names[i].length());
    }

    for(int i=0; i<names.size(); i++){
        print(pad(names[i], longest_props[i]+extra_pad), ""s);
    }
    print(horizontalGap ? ""s : "\n"s);

    for(int i=0; i<things.size(); i++){
        for(int j=0; j<things[i].size(); j++){
            print("Prepad 2: "s + to_string(longest_props[i]) + " @ "s + to_string(i));
            string padded_text = pad(things[i][j], longest_props[i]+extra_pad);
            print(things[i][j], ""s);
            print("Len: "s + to_string(i) + " "s + to_string(j) + " "s + to_string(things[i][j].length()));
        }
        print("");
    }

    return;
}

void downloadFile(string link, string filename){
    executeCommand({POWERSHELL, "-noprofile", "$ProgressPreference = 'SilentlyContinue'; Invoke-WebRequest "s + link + " -OutFile \""s + filename +"\""s});
}

string getenv(const string& varname){
    char* char_str;
    char_str = std::getenv(varname.c_str());
    if(!char_str){
        return ""s;
    }
    return string(char_str);
}

const char skip_error_print[] = "njhid798ABHIdbA&*dhb";

void clone_or_pull(string link){
    smatch sm;
    string folder;
    if(regex_search(link, sm, regex("\\/([^\\/]+)$"))){
        folder = sm[1];
    }else{
        throw skip_error_print;
    }

    if(!exists(folder)){
        executeCommand({"git", "clone", link});
    }else{
        executeCommand({"git", "pull", "./"s + folder});
    }
}

template<typename t>
vector<t> removeDuplicates(const vector<t> &v){
    vector<t> res; 
    for(int i=0; i<v.size(); i++){ // Doing this via set breaks for types that can't operator<
        bool found_dupli = false;
        for(int j=0; j<res.size(); j++){
            if(res[j] == v[i]){
                found_dupli = true;
                break;
            }
        }
        if(!found_dupli){
            res.push_back(v[i]);
        }
    }
    return res;
}

string getConda(){
    string condapath = "";
    try{
        auto f = ifstream("condapath.txt");
        getline(f, condapath);
        f.close();
        bool qs = condapath[0] == '"';
        bool qe = condapath[condapath.size()-1] == '"';
        if(qs || qe){
            condapath = condapath.substr(qs, condapath.size()-qe-qs);
        }
    }catch(...){}

    // Autodetect conda
    const string UserProfile = getenv("UserProfile"s);
    if (!exists(condapath)){
        const string Home = getenv("HOME"s);
        const string ProgramData = getenv("ProgramData"s);

        vector<string> conda_locs = {UserProfile + "\\miniconda3"s, UserProfile + "\\anaconda3"s, Home + "/anaconda3"s, Home + "/miniconda3"s};

        if(exists(ProgramData + "\\Microsoft\\Windows\\Start Menu\\Programs\\Anaconda3 (64-bit)")){
            string sh_args = readShortcut(ProgramData + "\\Microsoft\\Windows\\Start Menu\\Programs\\Anaconda3 (64-bit)\\Anaconda Prompt.lnk");
            smatch sm;
            if(regex_search(sh_args, sm, regex("([A-Z]:[\\w \\\\\\/]+)\\\\Scripts\\\\activate\\.bat"))){
                conda_locs.push_back(sm[1]);
            }
        }
        
        for (auto loc : conda_locs){
            if(exists(loc)){
                condapath = loc;
                break;
            }
        }
    }

    if (!exists(condapath)){
        print("Conda not found. If you already have Conda installed, create a text file called \"condapath.txt\" next to the installer and put conda's path in it.");
        print("You can download Conda from: https://docs.anaconda.com/miniconda/#latest-miniconda-installer-links");
        printColored("Please make sure to install conda in a location that has no spaces.", COLORS::White);
            if (UserProfile.find(" ") != string::npos){
                printColored("Your Windows username has spaces, as such, conda can not be automatically installed for you..", COLORS::Red);
                throw skip_error_print;
            }
            print("Would you like to have this script install conda for you, in "s +UserProfile + "?"s);
            int choice = promptForChoice("", "", {PFCType("Yes"), PFCType("No")})[0];
            if (choice == 0){
                print("Downloading...");
                executeCommand({CMD, "/C", "curl https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe -o miniconda.exe"});
                print("Installing...");
                executeCommand({CMD, "/C", "start /wait .\\miniconda.exe /S"});
                print("Cleaning up...");
                executeCommand({CMD, "/C", "del miniconda.exe"});
                print("Miniconda installed. Please run the script again.");
            }
        throw skip_error_print;
    }

    string conda_test_ver = "";
    bool conda_test_exc = false;
    try{
        executeCommandReadOutput({CMD, "/C", condapath + "\\Scripts\\activate.bat"s}); // Function does not directly feed strings into argv, but condapath has no spaces.
        conda_test_ver = executeCommandReadOutput({CMD, "/C", condapath + "\\Scripts\\activate.bat & conda -V"});
    }catch(...){
        conda_test_exc = true;
    }
        
    smatch sm;
    // Would conda have a version of the sort "conda 28.a7_big12.sub XXL"?
    if((!regex_search(conda_test_ver, sm, regex("conda [1-9].+"))) || conda_test_exc){
        print("\nCould not initialize conda.");
        if(conda_test_exc){
            print("Error when trying to activate it:");
            print(to_string(conda_test_exc));
            print("Please make sure that ", "");
            printColored(condapath + "\\Scripts\\activate.bat", "Cyan", false);
            print(" works, exists, and has no spaces in its path.");
            if(!exists(condapath + "\\Scripts\\activate.bat"s)){
                printColored("It does not exist.", "Red");
            }
            if(condapath.find(" ") != string::npos){
                printColored("Its path has spaces.", "Red");
            }
        }else{
            print("Conda version looks strange:");
            print(conda_test_ver);
            print("\nShould look something like:");
            print("conda 21.2.0");
        }
        throw skip_error_print;
    }

    return condapath;
}

//#define MAIN_TRYCATCH

int main(int argc, char* argv[]) {
    #ifdef MAIN_TRYCATCH
    try{
    #endif
        printColored("Script version: "s + VERSION, COLORS::DarkGreen);

        auto[is_dgpu, gpu_name] = get_gpu();
        string base_path = current_path().generic_string();
        path cwd = current_path();
        const string FOLDERNAME = "Comfy_Intel"s;
        const string CENVNAME = "cenv"s;
        
        //Conda/git checks
        print("Looking for conda...");
        string condapath = getConda();
        
        try{
            string o = executeCommandReadOutput({"git", "--version"});
        }catch(...){
            print("Git not found.");
            print("You can download Git from: https://git-scm.com/download/win");
            throw skip_error_print;
        }

        vector<PFCType> choices = {
            PFCType("Set up ComfyUI", "Download ComfyUI and install dependencies and other things needed to run on Intel Arc"),
            PFCType("Download a model")
        };

        int chosen_install = promptForChoice("---", "What to do?", choices, 0)[0];

        if (chosen_install == 0){

            //####################################
            //#         Install ComfyUI          #
            //####################################

            vector<string> gpu_text;
            if (is_dgpu){
                gpu_text = {"a"s, "discrete"s, gpu_name};
            }else{
                gpu_text = {"an"s, "integrated"s, gpu_name};
            }
            // Description of what is to be installed
            print("");
            printColored("A folder ", COLORS::Default, false);
            printColored(FOLDERNAME, COLORS::Cyan, false);

            if (!is_directory(FOLDERNAME)){
                printColored(" containing ComfyUI and Conda environment \""s + CENVNAME + "\" will be created in"s, COLORS::Default);
                printColored(base_path, COLORS::Cyan, false);
            }else{
                string conda_text = is_directory("./"s + FOLDERNAME + "/"s + CENVNAME) ? "updated"s : "created"s;
                printColored(" exists, the contained ComfyUI will be updated and Conda environment \""s + CENVNAME + "\" {conda_text}"s, COLORS::Default, false);
            }

            printColored(", \ninstalling IPEX for "s + gpu_text[0] + " "s, COLORS::Default, false);
            printColored(gpu_text[1], COLORS::Cyan, false);
            printColored(" "s + gpu_text[2] + " and using Conda at ", COLORS::Default, false);
            printColored(condapath, COLORS::Cyan, false);
            print(",\nas well as containing 1 batch file - used to launch Comfy (with --lowvram),");
            print("and a shortcut to it outside the folder.");
            print("\nContinue?");
            int c = promptForChoice("", "", {PFCType("Yes"), PFCType("No")})[0];
            if(c == 1){ // I don't want syntactic diabetes
                return 0;
            }

            struct custom_node{
                string Name, Description, link;
                custom_node(const string& _Name, const string& _Description, const string& _link): 
                    Name(_Name), Description(_Description), link(_link)
                {}
            };

            vector<custom_node> custom_nodes_info = {
                custom_node("GGUF"s,            "Flux.1 quantized below 8 bit, for Arc GPUs with <16GB of VRAM"s,  "https://github.com/city96/ComfyUI-GGUF"s),
                custom_node("BrushNet"s,        "More intelligent inpainting, and using any SD1.5/XL model"s,      "https://github.com/nullquant/ComfyUI-BrushNet"s),
                //custom_node("Impact Pack"s,     "Pack of nodes for object segmentation and dealing with masks"s,   "https://github.com/ltdrdata/ComfyUI-Impact-Pack"s),
                custom_node("SUPIR"s,           "High quality upscaling for realistic images"s,                    "https://github.com/kijai/ComfyUI-SUPIR"s),
                custom_node("KJNodes"s,         "Various misc. nodes"s,                                            "https://github.com/kijai/ComfyUI-KJNodes"s),
                custom_node("rghtree"s,         "Optimized execution, progressbar and various misc. nodes"s,       "https://github.com/rgthree/rgthree-comfy"s),
                custom_node("ExtraModels"s,     "Allows running additional non-SD models (such as Pixart)"s,       "https://github.com/city96/ComfyUI_ExtraModels"s),
                custom_node("IPAdapter Plus"s,  "Image Prompts"s,                                                  "https://github.com/cubiq/ComfyUI_IPAdapter_plus"s),
                custom_node("Controlnet aux"s,  "Additional Controlnet preprocessors"s,                            "https://github.com/Fannovel16/comfyui_controlnet_aux"s),
                custom_node("Tiled KSampler"s,  "KSampler for very large images"s,                                 "https://github.com/BlenderNeko/ComfyUI_TiledKSampler"s),
                //custom_node("Pysssss scripts"s, "Play sound node and other misc. nodes abd UI additions"s,         "https://github.com/pythongosssss/ComfyUI-Custom-Scripts"s),
            };
            vector<vector<string> > table_customnodes;
            for(auto cn : custom_nodes_info){
                table_customnodes.push_back({cn.Name, cn.Description});
            }
            
            print("Would you like to install all of the following custom nodes:\n");
            formatTable(table_customnodes, {"Name", "Description"});
            print("\nNote: Some of these require additional models to function, which you can download using this script after installing.");
            int chosen_custom_nodes = promptForChoice("", "", {PFCType("Yes"), PFCType("No")}, 0)[0];
        
            // Slightly more organized stuff
            if(!is_directory(FOLDERNAME)){
                create_directory(FOLDERNAME);
            }
            cwd /= FOLDERNAME;
            
            
            // ComfyUI, hijacks
            clone_or_pull("https://github.com/comfyanonymous/ComfyUI");
            cwd /= "ComfyUI/comfy";
            clone_or_pull("https://github.com/comfyanonymous/ComfyUI");
            clone_or_pull("https://github.com/Disty0/ipex_to_cuda");
            print("Not applying Disty's hijacks (temporarily!)");
            //replaceTextInFile("model_management.py", "as ipex\n", "as ipex#\n    from ipex_to_cuda import ipex_init\n    ipex_init()")
            cwd = cwd.parent_path().parent_path();
            
            // Install dependencies
            Conda conda(condapath);
            if (!is_directory(CENVNAME)){
                conda.command("conda create -p ./"s + CENVNAME + " python=3.10 -y"s);
            }
            conda.command("conda activate ./"s + CENVNAME);
            conda.command("conda install pkg-config libuv -y"s);
            conda.command("pip install -r ./ComfyUI/requirements.txt"s);

            if (chosen_custom_nodes == 0){
                cwd /= "ComfyUI/custom_nodes";
                for (auto cn : custom_nodes_info){
                    smatch sm;
                    string folder;
                    if(regex_search(cn.link, sm, regex("\\/([^\\/]+)$"))){
                        folder = sm[1];
                    }else{
                        throw "Could not do regex for custom node "s + cn.link;
                    }

                    clone_or_pull(cn.link);
                    if (exists(cwd / folder / "requirements.txt")){
                        conda.command("pip install -r ./ComfyUI/custom_nodes/" + folder + "/requirements.txt");
                    }
                }
                    
                //TODO: Implement Impact Pack setup
                cwd = cwd.parent_path().parent_path();
            }

            #ifdef _WIN32
            const string URL = is_dgpu ? "xpu"s : "mtl";

            conda.command("python -m pip install torch==2.3.1+cxx11.abi torchvision==0.18.1+cxx11.abi torchaudio==2.3.1+cxx11.abi intel-extension-for-pytorch==2.3.110+xpu \
                    --extra-index-url https://pytorch-extension.intel.com/release-whl/stable/"s + URL + "/us/");

            conda.command("pip install dpcpp-cpp-rt==2024.2.1 mkl-dpcpp==2024.2.1 onednn==2024.2.1");
            #else
            conda.command("python -m pip install torch==2.3.1+cxx11.abi torchvision==0.18.1+cxx11.abi torchaudio==2.3.1+cxx11.abi intel-extension-for-pytorch==2.3.110+xpu \
                            oneccl_bind_pt==2.3.100+xpu --extra-index-url https://pytorch-extension.intel.com/release-whl/stable/xpu/us/");
            #endif
            conda.command("pip install numpy==1.26.4");
            conda.command("pip install onnxruntime-openvino");
            
            
            // Create start script/s? Maybe 1 script + 1 shortcut only to not confuse people too much.
            const string _START_LOWVRAM_BASE_FILENAME="start_lowvram";
            #ifdef _WIN32
            const string start_lowvram_filename = _START_LOWVRAM_BASE_FILENAME + ".bat"s;
            const string start_lowvram_content = "call \"" + condapath + "\\Scripts\\activate.bat\" \ncd /D \"%~dp0\" \ncall conda activate ./"s + CENVNAME + " \ncd ./ComfyUI \npython ./main.py --bf16-unet --disable-ipex-optimize --lowvram";
            #else
            const string start_lowvram_filename = _START_LOWVRAM_BASE_FILENAME + ".sh"s;
            const string start_lowvram_content = "#!/bin/bash\n:("s;
            //TODO: Implement me
            #endif
            ofstream f(cwd / start_lowvram_filename);
            f.write(start_lowvram_content.c_str(), start_lowvram_content.size());
            f.close();
            
            
            // Shortcut/s?
            #ifdef _WIN32
            int retc = makeShortcut(base_path + "\\ComfyUI.lnk"s, "C:\\Windows\\System32\\cmd.exe", "/K `\""s + base_path + "\\"s + FOLDERNAME + "\\"s + start_lowvram_filename + "`\""s, "shell32.dll"s, 14);
            if (retc != 0){
                print("An error ocurred when creating shortcut (" + to_string(retc) + ").");
                throw skip_error_print;
            }
            #else
            //TODO: Implement me
            print(".desktop file code here");
            #endif

            conda.end();
            print(""); // TODO: Is flush needed here in C++?

            if (chosen_custom_nodes == 0){
                print("Applying SUPIR fixes...");
                replaceTextInFile("./cenv/lib/site-packages/open_clip/transformer.py", "x.to(torch.float32)", "x.to(self.weight.dtype)");
                replaceTextInFile("./ComfyUI/custom_nodes/ComfyUI-SUPIR/sgm/modules/diffusionmodules/sampling.py", "mps(device):", "mps(device) or comfy.model_management.is_intel_xpu():");
                print("Done.");
            }

            printColored("\n\nComfyUI is set up. Press enter to continue.\n", "Green");
        }else if(chosen_install == 1){

            //TODO: Split into different files?
            //##################################
            //#        Download a model        #
            //##################################

            if (!is_directory(FOLDERNAME)){
                print("Please install ComfyUI first.");
                throw skip_error_print;
            }

            struct DownloadableFile{
                string link;
                //In MB
                int size;
                string dir;
                string filename_override;
                DownloadableFile(const string &_link, int _size, const string &_dir = "checkpoints"s, const string &_filename_override = ""):
                    link(_link), size(_size), dir(_dir), filename_override(_filename_override)
                {}

                string get_filename() const {
                    if(filename_override != ""s){
                        return filename_override;
                    }else{
                        smatch sm;
                        if(regex_search(link, sm, regex("\\/([^\\/]+)$"))){
                            return sm[1];
                        }else{
                            print("Could not find filename for {model.link} ({model})");
                            throw skip_error_print;
                        }
                    }
                    return "";
                }

                bool operator==(const DownloadableFile& other) const {
                    return link==other.link && size==other.size && dir==other.dir && filename_override == other.filename_override;
                }
            };

            struct DownloadableCollection{
                vector<DownloadableFile> models;
                string name;
                string license;
                DownloadableCollection(const vector<DownloadableFile> &_models, const string &_name, const string &_license=""): 
                    models(_models), name(_name), license(_license) 
                {}
            };

            DownloadableFile t5_8               ("https://huggingface.co/comfyanonymous/flux_text_encoders/resolve/main/t5xxl_fp8_e4m3fn.safetensors", 4779, "clip");
            DownloadableFile clip_l             ("https://huggingface.co/comfyanonymous/flux_text_encoders/resolve/main/clip_l.safetensors", 346, "clip");
            DownloadableFile ae                 ("https://huggingface.co/black-forest-labs/FLUX.1-schnell/resolve/main/ae.safetensors", 327, "vae");
            DownloadableFile sdxl_vae_10        ("https://huggingface.co/stabilityai/sdxl-vae/resolve/main/sdxl_vae.safetensors", 359, "vae");

            DownloadableFile clip_tune_text      ("https://huggingface.co/zer0int/CLIP-GmP-ViT-L-14/resolve/main/ViT-L-14-TEXT-detail-improved-hiT-GmP-TE-only-HF.safetensors", 346);
            DownloadableFile clip_tune_smooth    ("https://huggingface.co/zer0int/CLIP-GmP-ViT-L-14/resolve/main/ViT-L-14-BEST-smooth-GmP-TE-only-HF-format.safetensors", 346);
            
            DownloadableFile fluxdev8b_u         ("https://huggingface.co/Kijai/flux-fp8/resolve/main/flux1-dev-fp8-e4m3fn.safetensors", 11621, "unet");
            DownloadableFile fluxschnell8b_u     ("https://huggingface.co/Kijai/flux-fp8/resolve/main/flux1-schnell-fp8-e4m3fn.safetensors", 11621, "unet");
            DownloadableFile fluxdev4b_u         ("https://huggingface.co/city96/FLUX.1-dev-gguf/resolve/main/flux1-dev-Q4_0.gguf", 6632, "unet");
            DownloadableFile fluxschnell4b_u     ("https://huggingface.co/city96/FLUX.1-schnell-gguf/resolve/main/flux1-schnell-Q4_0.gguf", 6632, "unet");

            DownloadableFile sdxl_10_sft         ("https://huggingface.co/stabilityai/stable-diffusion-xl-base-1.0/resolve/main/sd_xl_base_1.0.safetensors", 6775);
            DownloadableFile jugxl_v9_rdp2_sft   ("https://huggingface.co/RunDiffusion/Juggernaut-XL-v9/resolve/main/Juggernaut-XL_v9_RunDiffusionPhoto_v2.safetensors", 6775);
            DownloadableFile jugxl_rdp2li_sft    ("https://huggingface.co/RunDiffusion/Juggernaut-XL-Lightning/resolve/main/Juggernaut_RunDiffusionPhoto2_Lightning_4Steps.safetensors", 7634);
            DownloadableFile jugxl_v2_sft        ("https://huggingface.co/RunDiffusion/Juggernaut-XL/resolve/main/juggernautXL_version2.safetensors", 6775);
            DownloadableFile animagine_31_sft    ("https://huggingface.co/cagliostrolab/animagine-xl-3.1/resolve/main/animagine-xl-3.1.safetensors", 6775);

            DownloadableFile supir_f             ("https://huggingface.co/Kijai/SUPIR_pruned/resolve/main/SUPIR-v0F_fp16.safetensors", 2856);
            DownloadableFile supir_q             ("https://huggingface.co/Kijai/SUPIR_pruned/resolve/main/SUPIR-v0Q_fp16.safetensors", 2856);

            DownloadableFile powerpaint_1        ("https://huggingface.co/JunhaoZhuang/PowerPaint-v2-1/resolve/main/PowerPaint_Brushnet/diffusion_pytorch_model.safetensors", 3801, "inpaint/powerpaint", "powerpaint_v2.safetensors");
            DownloadableFile powerpaint_2        ("https://huggingface.co/JunhaoZhuang/PowerPaint-v2-1/resolve/main/PowerPaint_Brushnet/pytorch_model.bin", 528, "inpaint/powerpaint");

            DownloadableFile brushnet_15_seg     ("https://drive.google.com/file/d/1uLQ55rdcFljdP_9iYGX5ZmJWzXFJast7/view?usp=drive_link", 2310, "inpaint/brushnet", "segmentation_mask.safetensors");
            DownloadableFile brushnet_15_ran     ("https://drive.google.com/file/d/1h_WOUK1SyV_YwoXAT7rRO012dI5sm3OE/view?usp=drive_link", 2310, "inpaint/brushnet", "random_mask.safetensors");

            DownloadableFile brushnet_xl_seg     ("https://drive.google.com/file/d/1yPhfQ3y60fXURr9_YfQrdE48y68IZjBN/view?usp=drive_link", 1390, "inpaint/brushnet_xl", "segmentation_mask.safetensors");
            DownloadableFile brushnet_xl_ran     ("https://drive.google.com/file/d/1e-vXMnlnEGmOr_s5P_hT0Uqp0nPuGFkT/view?usp=drive_link", 1390, "inpaint/brushnet_xl", "random_mask.safetensors");

            DownloadableFile pixart_sigma_base   ("https://huggingface.co/PixArt-alpha/PixArt-Sigma/resolve/main/PixArt-Sigma-XL-2-1024-MS.pth", 2652);

            DownloadableFile dreamshaper_8_ba    ("https://civitai.com/api/download/models/128713?type=Model&format=SafeTensor&size=pruned&fp=fp16", 2008, "checkpoints", "dreamshaper_8.safetensors");
            DownloadableFile drmsh8r_8_inp_ba    ("https://civitai.com/api/download/models/131004?type=Model&format=SafeTensor&size=pruned&fp=fp16", 2008, "checkpoints", "dreamshaper_8_inpainting.safetensors");
            
            DownloadableCollection fluxdev4bit         ({ae, t5_8, clip_l, fluxdev4b_u}, "Flux.1 Dev 4-bit", "https://huggingface.co/black-forest-labs/FLUX.1-dev/blob/main/LICENSE.md");
            DownloadableCollection fluxschnell4bit     ({ae, t5_8, clip_l, fluxschnell4b_u}, "Flux.1 Schnell 4-bit");
            DownloadableCollection supir               ({supir_f, supir_q}, "SUPIR");
            DownloadableCollection powerpaint          ({powerpaint_1, powerpaint_2}, "PowerPaint (better Brushnet, SD1.5)");
            DownloadableCollection brushnet_15         ({brushnet_15_ran, brushnet_15_seg}, "Brushnet (SD1.5)");
            DownloadableCollection brushnet_xl         ({brushnet_xl_ran, brushnet_xl_seg}, "Brushnet XL");
            DownloadableCollection sdxl_10             ({sdxl_10_sft}, "SDXL 1.0");
            DownloadableCollection jugxl_v9_rdp2       ({jugxl_v9_rdp2_sft}, "Juggernaut XL v9 RunDiffusion Photo v2");
            DownloadableCollection jugxl_v9_rdp2_li    ({jugxl_rdp2li_sft}, "Juggernaut XL v9 RDP v2 Lightning 4-step");
            DownloadableCollection jugxl_v2            ({jugxl_v2_sft}, "Juggernaut XL v2");
            DownloadableCollection animagine_31        ({animagine_31_sft}, "Animagine XL 3.1");
            DownloadableCollection fluxdev8bit         ({ae, t5_8, clip_l, fluxdev8b_u}, "Flux.1 Dev 8-bit", "https://huggingface.co/black-forest-labs/FLUX.1-dev/blob/main/LICENSE.md");
            DownloadableCollection fluxschnell8bit     ({ae, t5_8, clip_l, fluxschnell8b_u}, "Flux.1 Schnell 8-bit");
            DownloadableCollection pixart_sigma        ({sdxl_vae_10, t5_8, pixart_sigma_base}, "Pixart Sigma");
            DownloadableCollection dreamshaper_8       ({dreamshaper_8_ba}, "Dreamshaper 8 (SD1.5)");
            DownloadableCollection dreamshaper_8_inp   ({drmsh8r_8_inp_ba}, "Dreamshaper 8 Inpainting (SD1.5)");
            DownloadableCollection clip_finetunes      ({clip_tune_smooth, clip_tune_text}, "CLIP-L finetunes by zer0int");

            vector<DownloadableCollection> collections = {fluxdev4bit, fluxschnell4bit, supir, powerpaint, brushnet_15, brushnet_xl, sdxl_10, jugxl_v9_rdp2, 
                jugxl_v9_rdp2_li, jugxl_v2, animagine_31, pixart_sigma, fluxdev8bit, fluxschnell8bit, dreamshaper_8, dreamshaper_8_inp, clip_finetunes};

            cwd /= FOLDERNAME;

            print("Which model would you like to download?\n");

            vector<vector<string> > table;
            vector<PFCType> choices;
            for(int i=0; i<collections.size(); i++){
                vector<string> row;
                auto coll = collections[i];
                row[0] = pad(to_string(i+1) + ".", 4) + coll.name;
                float size_part = 0;
                float size_base = 0;
                for (auto model : coll.models){
                    if (find({ae, t5_8, clip_l}, model) != -1){
                        size_part += model.size;
                    }else{
                        size_base += model.size;
                    }
                }
                string size_str = to_string(size_base / 1000.0).substr(0, 4) + "GB";
                if (size_part > 0){
                    size_str += " + " + to_string(size_part / 1000.0).substr(0, 4) + "GB";
                }
                row[1] = size_str;
                table.push_back(row);
                choices.push_back(PFCType(to_string(i+1), coll.name, to_string(i+1)));
            }
            
            formatTable(table, {"Name", "Size"});

            print("\nAll Flux/Pixart models are downloaded piecemeal (separate VAE, FP8 T5, CLIP, UNET)");
            print("The VAE, T5 and CLIP can be used for all Flux models and account for 5.34GB.");
            print("The SDXL VAE and T5 can be used for Pixart Sigma.");
            print("They will not be redownloaded if you already have them, to save space.");

            print("\nType multiple numbers separated by spaces to download many models at once.");

            vector<int> ids = promptForChoice("", "", choices, 0, true);
            cwd /= "ComfyUI" / path("models");

            vector<DownloadableFile> models;
            for(auto id : ids){
                for(auto model : collections[id].models){
                    models.push_back(model);
                }
            }
            models = removeDuplicates(models);
            float sum = 0;
            for (auto model : models){
                if (!exists(cwd / model.dir / model.get_filename())){
                    sum += model.size;
                }
            }
            if (sum < 1){
                print("All chosen models are already downloaded.");
                return 0;
            }
            if(models.size() > 1){
                print("About to download "s + to_string(sum / 1000.0).substr(0, 4) + "GB, continue?"s);
                int cont = promptForChoice("", "", {PFCType("Yes"), PFCType("No")})[0];
                if (cont == 1){
                    return 0;
                }
            }

            // Don't randomly decide to ask for license agreeing mid-download because user decided to choose them out of order
            for (int id : ids){
                auto collection = collections[id];
                if (collection.license != ""s){
                    int agree = promptForChoice("Please review and agree with the "s + collection.name + " model's license, which can be found at:"s, 
                        collection.license + "\n(ctrl+left click a link to open in browser)"s, {PFCType("Agree"), PFCType("Disagree")})[0];
                    if(agree != 0){
                        return 0;
                    }
                }
            }

            for(int id : ids){
                auto collection = collections[id];
                printColored("\nDownloading " + collection.name + "."s, COLORS::White);
                for (auto model : collection.models){
                    string filename = model.get_filename();

                    std::error_code ec;
                    create_directories(cwd / model.dir, ec);
                    if (exists(cwd / model.dir / filename)){
                        print("File "s + filename + " ("s + model.link + ")\nalready exists..."s);
                    }else{
                        print("\nFile "s + filename + ", "s + to_string(model.size) + "MB, link:\n"s + model.link);
                        print("Downloading...", ""); // TODO: Is flush needed here in C++?

                        auto old_cwd = cwd;
                        cwd /= model.dir;
                        downloadFile(model.link, (cwd / filename).generic_string());
                        cwd = old_cwd;
                    }
                }
            }

            string plural = ids.size() > 1 ? "s" : "";
            printColored("\n\nFinished downloading model"s + plural + ". Press enter to continue.\n"s, COLORS::Green);
        }
    #ifdef MAIN_TRYCATCH
    }catch(const std::exception& e){
        if (!strcmp(e.what(), skip_error_print) == 0){
            printColored("An error occurred:", COLORS::Red);
            print(e.what());
        }
        //pdb.set_trace()
        print("\nPress enter to continue...");
    }
    #endif
    input();
    return 0;
}
#include <ESP.h>
#include <layout.h>
#include <imports.h>
#include <jni.h>
#include "KittyMemory/KittyMemory.hpp"
#include "KittyMemory/MemoryPatch.hpp"

#include <telaonline.h>
class _BYTE; class _QWORD;
class _DWORD; class _WORD;
#define _QWORD long 
#define _DWORD long
#define _BYTE long 
#define _WORD long
#include <EGL/egl.h>



EGLBoolean(*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
	eglQuerySurface(dpy, surface, EGL_WIDTH, &egl.width);
	eglQuerySurface(dpy, surface, EGL_HEIGHT, &egl.height);
	if (!egl.setup) {
		ImGui::CreateContext();
		SetupImgui(); 
        InitTexture();
		egl.setup = true;
	}
	ImGuiIO& io = ImGui::GetIO(); 
    ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplAndroid_NewFrame(egl.width, egl.height);
	ImGui::NewFrame();
    
	int touchCount = (((int (*)())(Il2CppGetMethodOffset(OBFUSCATE("UnityEngine.InputLegacyModule.dll"), OBFUSCATE("UnityEngine"), OBFUSCATE("Input"), OBFUSCATE("get_touchCount"))))());
    if (touchCount > 0) {
        UnityEngine_Touch_Fields touch = ((UnityEngine_Touch_Fields(*)(int))(Il2CppGetMethodOffset(OBFUSCATE("UnityEngine.InputLegacyModule.dll"), OBFUSCATE("UnityEngine"), OBFUSCATE("Input"), OBFUSCATE("GetTouch"), 1)))(0);
        float reverseY = io.DisplaySize.y - touch.m_Position.fields.y;
        io.MousePos = ImVec2(touch.m_Position.fields.x, reverseY);
        io.MouseDown[0] = true;
    } else {
        io.MouseDown[0] = false;
    }
    
	menu(); 
    DrawESP(egl.width, egl.height); 
    Aimbott();
    
	ImGui::EndFrame();
    ImGui::Render(); 
	glViewport(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	return old_eglSwapBuffers(dpy, surface);
}



void* loadCheat(void* org) {
while (!getLibraryLoaded(OBFUSCATE("libil2cpp.so")))
sleep(10);
Il2CppAttach();    

	// 大厅
// 补丁已关闭
//  MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"),OBFUSCATE("ffantihack"),OBFUSCATE("DAPAGLLINEL"),OBFUSCATE("DHEHFAGCCDM"),1),OBFUSCATE("C0 03 5F D6")).Modify();
    
    
// 补丁已关闭
// MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("DAPAGLLINEL"), OBFUSCATE("DHEHFAGCCDM"), 1), OBFUSCATE("C0 03 5F D6")).Modify();
// MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("DAPAGLLINEL"), OBFUSCATE("JGFDFDBKIGD"), 1), OBFUSCATE("C0 03 5F D6")).Modify();
// MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("DAPAGLLINEL"), OBFUSCATE("INICGCCOGEC"), 1), OBFUSCATE("C0 03 5F D6")).Modify();
// MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("DAPAGLLINEL"), OBFUSCATE("JHACIFHANIL"), 0), OBFUSCATE("C0 03 5F D6")).Modify();
// MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("DAPAGLLINEL"), OBFUSCATE("HIAKKLNIKFM"), 3), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("DAPAGLLINEL"), OBFUSCATE("ACLNMEMGMNH"), 5), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("DAPAGLLINEL"), OBFUSCATE("GPCNDKMBNMC"), 0), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();

MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("CLFIIKMGKHI"), OBFUSCATE("BGFDKDLDEFB"), 2), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("CLFIIKMGKHI"), OBFUSCATE("GPCNDKMBNMC"), 0), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("CLFIIKMGKHI"), OBFUSCATE("GKHNPBAHLHE"), 4), OBFUSCATE("C0 03 5F D6")).Modify();
// 补丁已关闭
// MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("CLFIIKMGKHI"), OBFUSCATE("PCEAEHNPBGI"), 1), OBFUSCATE("C0 03 5F D6")).Modify();
// MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("CLFIIKMGKHI"), OBFUSCATE("DLIDMJONJKE"), 1), OBFUSCATE("C0 03 5F D6")).Modify();
// MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("ffantihack"), OBFUSCATE("CLFIIKMGKHI"), OBFUSCATE("DAFGLNFFCML"), 1), OBFUSCATE("C0 03 5F D6")).Modify();

MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("GameRegionSwitch"), OBFUSCATE("IsBannedBRTrioRank"), 0), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("GameRegionSwitch"), OBFUSCATE("IsBannedCSTrioRank"), 0), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("UIModelGroup"), OBFUSCATE("get_IsBannedTrioRank"), 0), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("UIModelGroup"), OBFUSCATE("set_IsBannedTrioRank"), 1), OBFUSCATE("C0 03 5F D6")).Modify();

MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("UIModelRecruit"), OBFUSCATE("IsAccountRecruitBanned"), 0), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("UIModelRecruit"), OBFUSCATE("CheckAccountRecruitBanAndShowTips"), 0), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("UIModelRecruit"), OBFUSCATE("GetAccountRecruitBanRemainingMinutes"), 0), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("UIModelRecruit"), OBFUSCATE("ShowAccountRecruitBannedTips"), 0), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("UIModelRecruit"), OBFUSCATE("ProcessAccountRecruitBannedNtf"), 1), OBFUSCATE("C0 03 5F D6")).Modify();

MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("BanReasonDataManager"), OBFUSCATE("GetBanReasonDesc"), 2), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("BanReasonDataManager"), OBFUSCATE("GetBanReasonUrl"), 1), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("BanReasonDataManager"), OBFUSCATE("GetDefaultUrl"), 1), OBFUSCATE("00 00 80 D2 C0 03 5F D6")).Modify();

MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogReportCheat"), 10), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogReportCheatInHistory"), 8), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogOutGameReport"), 4), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogClanReport"), 5), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogAndroidApplicationDetection"), 1), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("SendEventTypeHackClientFix"), 1), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogEventTypeBannedPlayerAppeal"), 4), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogEventTypeAntiAddictionBanAck_IP_US"), 1), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogEventTypeAntiAddictionBan"), 2), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("SendUGCColliderResourceCheck"), 2), OBFUSCATE("C0 03 5F D6")).Modify();

MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("RecordSpecialKill"), 2), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("SendSpecialKillLog"), 0), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogEventTypeBannedPlayerAppeal"), 4), OBFUSCATE("C0 03 5F D6")).Modify();
// 补丁已关闭
// MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogAngelDevilBubbleClick"), 3), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogAIAssistantFAQ"), 8), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogScanQRCodeInfo"), 5), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("CheckAndSendQuitTimes"), 0), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("SendMicAccessPopUp"), 1), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("RecordCDNDownloadInfo"), 4), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogTargetTractionMission"), 4), OBFUSCATE("C0 03 5F D6")).Modify();
MemoryPatch::createWithHex((uintptr_t)Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW"), OBFUSCATE("EventLogger"), OBFUSCATE("LogUGCMapTranslationFail"), 2), OBFUSCATE("C0 03 5F D6")).Modify();








A64HookFunction(Il2CppGetMethodOffset(OBFUSCATE("Assembly-CSharp.dll"), OBFUSCATE("COW.GamePlay"), OBFUSCATE("Player"), OBFUSCATE("UpdateBehavior"), 2), (void *) _LateUpdate, (void **) &LateUpdate);
    
    
    
    if (mlovinit()) {
        setShader(OBFUSCATE("_AlphaMask"));
        LogShaders(); Wallhack();
    }
	
void* libEGL = dlopen(OBFUSCATE("libEGL.so"), RTLD_NOW | RTLD_GLOBAL);
if (!libEGL) return nullptr;
A64HookFunction((void*)dlsym(libEGL, OBFUSCATE("eglSwapBuffers")),(void*)hook_eglSwapBuffers,(void**)&old_eglSwapBuffers);
return nullptr;
}
static pthread_once_t rapz_once = PTHREAD_ONCE_INIT;

static void rapz_start() {
    pthread_t ptid;
    pthread_create(&ptid, nullptr, loadCheat, nullptr);
    pthread_detach(ptid);
}

__attribute__((constructor)) static void rapz_auto_start() {
    pthread_once(&rapz_once, rapz_start);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    rapz_auto_start();
    return JNI_VERSION_1_6;
}

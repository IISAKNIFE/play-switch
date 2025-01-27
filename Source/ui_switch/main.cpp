#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <switch.h>

#include "Log.h"
#include "AppConfig.h"
#include "PS2VM.h"
#include "DiskUtils.h"
#include "PH_Generic.h"
#include "GSH_Deko3d.h"

#include "PS2VM_Preferences.h"

#define PLAY_PATH	"/switch/Play"
#define DEFAULT_FILE "/switch/Play/test.elf"

#define EXIT_COMBO (HidNpadButton_Plus | HidNpadButton_R)

static bool IsBootableExecutablePath(const fs::path& filePath)
{
	auto extension = filePath.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	return (extension == ".elf");
}

static bool IsBootableDiscImagePath(const fs::path& filePath)
{
	const auto& supportedExtensions = DiskUtils::GetSupportedExtensions();
	auto extension = filePath.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	auto extensionIterator = supportedExtensions.find(extension);
	return extensionIterator != std::end(supportedExtensions);
}

int main(int argc, char** argv)
{
	CPS2VM* m_virtualMachine = nullptr;
	bool executionOver = false;
	const char* file;
	fs::path filePath;
	PadState pad;
	int i;

	Framework::PathUtils::SetFilesDirPath(PLAY_PATH);
	Framework::PathUtils::SetCacheDirPath(PLAY_PATH);


	//consoleDebugInit(debugDevice_SVC);
	//inet_aton("127.0.0.1", &__nxlink_host);
	socketInitializeDefault();
	nxlinkStdio();
	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&pad);

	fprintf(stderr, "Play! " PLAY_VERSION "\n");
	fprintf(stderr, "argc: %d\n", argc);
	for(i = 0; i < argc; i++)
	{
		fprintf(stderr, "argv[%d]: %s\n", i, argv[i]);
	}

	// load from argv if present
	if(argc > 1)
	{
		file = argv[1];
	}
	else
	{
		file = DEFAULT_FILE;
	}

	filePath = file;
	fprintf(stderr, "Booting file: '%s'\n", file);

	if(IsBootableDiscImagePath(filePath))
	{
		fprintf(stderr, "Is Bootable Disc Image");
		CAppConfig::GetInstance().SetPreferencePath(PREF_PS2_CDROM0_PATH, filePath);
		CAppConfig::GetInstance().Save();
	}

	CAppConfig::GetInstance().SetPreferenceBoolean("log.showprints", true);

	m_virtualMachine = new CPS2VM();
	m_virtualMachine->Initialize();
	m_virtualMachine->CreatePadHandler(CPH_Generic::GetFactoryFunction());
	m_virtualMachine->CreateGSHandler(CGSH_Deko3d::GetFactoryFunction());
	auto connection = m_virtualMachine->m_ee->m_os->OnRequestExit.Connect(
	    [&executionOver]() {
		    executionOver = true;
	    });

	fprintf(stderr, "Starting execution...\n");
	try
	{
		if(IsBootableExecutablePath(filePath))
		{
			fprintf(stderr, "Booting ELF File\n");
			m_virtualMachine->m_ee->m_os->BootFromFile(filePath);
		}
		else if(IsBootableDiscImagePath(filePath))
		{
			fprintf(stderr, "Booting CD ROM\n");
			m_virtualMachine->m_ee->m_os->BootFromCDROM();
		}
	}
	catch(const std::runtime_error& e)
	{
		fprintf(stderr, "Exception: %s\n", e.what());
		goto done;
	}
	catch(...)
	{
		fprintf(stderr, "Unknown exception\n");
		goto done;
	}

	fprintf(stderr, "Loaded! Starting execution...\n");

	m_virtualMachine->Resume();

	while(appletMainLoop() && !executionOver)
	{
		padUpdate(&pad);

		u64 buttons = padGetButtons(&pad);
		if((buttons & EXIT_COMBO) == EXIT_COMBO)
			executionOver = true;

		auto padHandler = static_cast<CPH_Generic*>(m_virtualMachine->GetPadHandler());

		//padHandler->SetAxisState(PS2::CControllerInfo::ANALOG_LEFT_X, (pad.lx / 255.f) * 2.f - 1.f);
		//padHandler->SetAxisState(PS2::CControllerInfo::ANALOG_LEFT_Y, (pad.ly / 255.f) * 2.f - 1.f);
		//padHandler->SetAxisState(PS2::CControllerInfo::ANALOG_RIGHT_X, (pad.rx / 255.f) * 2.f - 1.f);
		//padHandler->SetAxisState(PS2::CControllerInfo::ANALOG_RIGHT_Y, (ry / 255.f) * 2.f - 1.f);
		padHandler->SetButtonState(PS2::CControllerInfo::DPAD_UP, buttons & HidNpadButton_Up);
		padHandler->SetButtonState(PS2::CControllerInfo::DPAD_DOWN, buttons & HidNpadButton_Down);
		padHandler->SetButtonState(PS2::CControllerInfo::DPAD_LEFT, buttons & HidNpadButton_Left);
		padHandler->SetButtonState(PS2::CControllerInfo::DPAD_RIGHT, buttons & HidNpadButton_Right);
		padHandler->SetButtonState(PS2::CControllerInfo::SELECT, buttons & HidNpadButton_Minus);
		padHandler->SetButtonState(PS2::CControllerInfo::START, buttons & HidNpadButton_Plus);
		padHandler->SetButtonState(PS2::CControllerInfo::SQUARE, buttons & HidNpadButton_Y);
		padHandler->SetButtonState(PS2::CControllerInfo::TRIANGLE, buttons & HidNpadButton_X);
		padHandler->SetButtonState(PS2::CControllerInfo::CIRCLE, buttons & HidNpadButton_A);
		padHandler->SetButtonState(PS2::CControllerInfo::CROSS, buttons & HidNpadButton_B);
		padHandler->SetButtonState(PS2::CControllerInfo::L1, buttons & HidNpadButton_L);
		padHandler->SetButtonState(PS2::CControllerInfo::R1, buttons & HidNpadButton_R);
	}

done:
	fprintf(stderr, "Finish\n");

	if(m_virtualMachine)
	{
		m_virtualMachine->Pause();
		m_virtualMachine->DestroyPadHandler();
		m_virtualMachine->DestroyGSHandler();
		//m_virtualMachine->DestroySoundHandler();
		m_virtualMachine->Destroy();
		delete m_virtualMachine;
		m_virtualMachine = nullptr;
	}

	socketExit();

	return 0;
}

//
// Copyright(c) 2016-2017 benikabocha.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
//

#include <Saba/Base/Log.h>

#include <Saba/Viewer/Viewer.h>
#include <Saba/Viewer/ViewerCommand.h>

#include <picojson.h>
#include <sol.hpp>
#include <fstream>
#include <vector>

namespace
{
	/*
	@brief	"init.json" から初期化設定を読み込む
	*/
	void ReadInitParameterFromJson(
		mmd::Viewer::InitializeParameter&	viewerInitParam,
		std::vector<mmd::ViewerCommand>&	viewerCommands
	)
	{
		bool msaaEnable = true;
		int msaaCount = 4;
		std::ifstream initJsonFile;
		initJsonFile.open("init.json");
		viewerInitParam.m_msaaEnable = true;
		viewerInitParam.m_msaaCount = 8;
		if (initJsonFile.is_open())
		{
			picojson::value val;
			initJsonFile >> val;
			initJsonFile.close();

			auto& init = val.get<picojson::object>();
			if (init["MSAAEnable"].is<bool>())
			{
				viewerInitParam.m_msaaEnable = init["MSAAEnable"].get<bool>();
			}
			if (init["MSAACount"].is<double>())
			{
				double count = init["MSAACount"].get<double>();
				viewerInitParam.m_msaaCount = (int)(count + 0.5);
			}
			if (init["Commands"].is<picojson::array>())
			{
				viewerCommands.clear();
				for (auto& command : init["Commands"].get<picojson::array>())
				{
					if (command.is<picojson::object>())
					{
						mmd::ViewerCommand viewerCmd;
						auto& cmdObj = command.get<picojson::object>();
						if (cmdObj["Cmd"].is<std::string>())
						{
							viewerCmd.SetCommand(cmdObj["Cmd"].get<std::string>());
						}
						if (cmdObj["Args"].is<picojson::array>())
						{
							for (auto& arg : cmdObj["Args"].get<picojson::array>())
							{
								if (arg.is<std::string>())
								{
									viewerCmd.AddArg(arg.get<std::string>());
								}
							}
						}
						viewerCommands.emplace_back(std::move(viewerCmd));
					}
				}
			}
		}
	}

	/*
	@brief	"init.lua" から初期化設定を読み込む
	*/
	void ReadInitParameterFromLua(
		const std::vector<std::string>&		args,
		mmd::Viewer::InitializeParameter&	viewerInitParam,
		std::vector<mmd::ViewerCommand>&	viewerCommands
	)
	{

		try
		{
			sol::state lua;
			lua.open_libraries(sol::lib::base, sol::lib::package);
			auto result = lua.load_file("init.lua");
			if (result)
			{
				sol::table argsTable = lua.create_table(args.size(), 0);
				for (const auto& arg : args)
				{
					argsTable.add(arg.c_str());
				}
				lua["Args"] = argsTable;

				result();

				auto msaa = lua["MSAA"];
				if (msaa)
				{
					viewerInitParam.m_msaaEnable = msaa["Enable"].get_or(false);
					viewerInitParam.m_msaaCount = msaa["Count"].get_or(4);
				}

				auto initCamera = lua["InitCamera"];
				if (initCamera)
				{
					bool useInitCamera = true;
					auto center = initCamera["Center"];
					auto eye = initCamera["Eye"];
					auto nearClip = initCamera["NearClip"];
					auto farClip = initCamera["FarClip"];
					auto radius = initCamera["Radius"];
					if (center.valid() &&
						eye.valid() &&
						nearClip.valid() &&
						farClip.valid() &&
						radius.valid()
						)
					{
						if (center["x"].valid() && center["y"].valid() && center["z"].valid())
						{
							viewerInitParam.m_initCameraCenter.x = center["x"].get<float>();
							viewerInitParam.m_initCameraCenter.y = center["y"].get<float>();
							viewerInitParam.m_initCameraCenter.z = center["z"].get<float>();
						}
						else
						{
							useInitCamera = false;
						}

						if (eye["x"].valid() && eye["y"].valid() && eye["z"].valid())
						{
							viewerInitParam.m_initCameraEye.x = eye["x"].get<float>();
							viewerInitParam.m_initCameraEye.y = eye["y"].get<float>();
							viewerInitParam.m_initCameraEye.z = eye["z"].get<float>();
						}
						else
						{
							useInitCamera = false;
						}

						viewerInitParam.m_initCameraNearClip = nearClip.get<float>();
						viewerInitParam.m_initCameraFarClip = farClip.get<float>();

						viewerInitParam.m_initCameraRadius = radius.get<float>();

						viewerInitParam.m_initCamera = useInitCamera;
					}

					auto initScene = lua["InitScene"];
					if (initScene)
					{
						auto unitScale = initScene["UnitScale"];
						if (unitScale.valid())
						{
							viewerInitParam.m_initSceneUnitScale = unitScale.get<float>();

							viewerInitParam.m_initScene = true;
						}
					}
				}

				sol::object commandsObj = lua["Commands"];
				if (commandsObj.is<sol::table>())
				{
					sol::table commands = commandsObj;
					viewerCommands.clear();
					for (auto cmdIt = commands.begin(); cmdIt != commands.end(); ++cmdIt)
					{
						sol::table cmd = (*cmdIt).second;
						mmd::ViewerCommand viewerCmd;
						std::string cmdText = cmd["Cmd"].get_or(std::string(""));
						viewerCmd.SetCommand(cmdText);
						sol::object argsObj = cmd["Args"];
						if (argsObj.is<sol::table>())
						{
							sol::table args = argsObj;
							for (auto argIt = args.begin(); argIt != args.end(); ++argIt)
							{
								std::string argText = (*argIt).second.as<std::string>();
								viewerCmd.AddArg(argText);
							}
						}
						viewerCommands.emplace_back(viewerCmd);
					}
				}
			}
		}
		catch (sol::error e)
		{
			SABA_ERROR("Failed to load init.lua.\n{}", e.what());
		}
	}
} // namespace

int SabaViewerMain(const std::vector<std::string>& args)
{
	INFO("Start");
	mmd::Viewer viewer;
	mmd::Viewer::InitializeParameter	viewerInitParam;
	std::vector<mmd::ViewerCommand>	viewerCommands;

	ReadInitParameterFromJson(viewerInitParam, viewerCommands);
	ReadInitParameterFromLua(args, viewerInitParam, viewerCommands);

	if (viewerInitParam.m_msaaEnable)
	{
		INFO("Enable MSAA");

		if (viewerInitParam.m_msaaCount != 2 &&
			viewerInitParam.m_msaaCount != 4 &&
			viewerInitParam.m_msaaCount != 8
			)
		{
			WARN("MSAA Count Force Change : 4");
			viewerInitParam.m_msaaCount = 4;
		}

		INFO("MSAA Count : {}", viewerInitParam.m_msaaCount);
	}
	else
	{
		INFO("Disable MSAA");
	}

	if (!viewer.Initialize(viewerInitParam))
	{
		return -1;
	}

	for (const auto& viewerCommand : viewerCommands)
	{
		viewer.ExecuteCommand(viewerCommand);
	}
	viewer.m_bgColor1.r = 1;
	viewer.m_bgColor1.g = 1;
	viewer.m_bgColor1.b = 1;
	viewer.m_gridEnabled = false;




	int ret = viewer.Run();

	viewer.Uninitislize();

	INFO("Exit");

	return ret;
}


#if _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

int main(int argc, char** argv)
{
	std::vector<std::string> args(argc);
#if _WIN32
	{
		WCHAR* cmdline = GetCommandLineW();
		int wArgc;
		WCHAR** wArgs = CommandLineToArgvW(cmdline, &wArgc);
		for (int i = 0; i < argc; i++)
		{
			args[i] = mmd::ToUtf8String(wArgs[i]);
		}
	}
#else // _WIN32
	for (int i = 0; i < argc; i++)
	{
		args[i] = argv[i];
	}
#endif

	auto ret = SabaViewerMain(args);
	mmd::SingletonFinalizer::Finalize();
	return ret;
}


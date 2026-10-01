// Copyright Epic Games, Inc. All Rights Reserved.

#include "DARK.h"
#include "Modules/ModuleManager.h"
#include "Internationalization/StringTableRegistry.h"

/**
 * Модуль игры. При старте регистрирует таблицу строк ST_UI из CSV
 * (Content/Localization/ST_UI.csv, исходный язык — русский). Так тексты работают сразу,
 * без ручного импорта String Table в редакторе, а сборщик локализации (Localization
 * Dashboard) находит эту таблицу по макросу ниже и даёт перевести её на английский.
 */
class FDARKModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		LOCTABLE_FROMFILE_GAME("ST_UI", "ST_UI", "Localization/ST_UI.csv");
	}

	virtual void ShutdownModule() override
	{
		FStringTableRegistry::Get().UnregisterStringTable(TEXT("ST_UI"));
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FDARKModule, DARK, "DARK");

DEFINE_LOG_CATEGORY(LogDARK)

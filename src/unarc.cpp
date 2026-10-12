// to do: отбор файлов по именам ("name" или "dir/name"),
//        дешифрование данных/заголовка
//        добавление ".arc", listfiles/-ap/-kb

// Обработка сбоев при распаковке архива
#undef  ON_CHECK_FAIL
#define ON_CHECK_FAIL()   UnarcQuit()
void UnarcQuit();

// Доступ к структуре архива, парсингу командной строки и выполнению операций над архивом
#include "ArcStructure.h"
#include "ArcCommand.h"
#include "ArcProcess.h"

// Экстренный выход из программы в случае ошибки
void UnarcQuit()
{
  CurrentProcess->quit(FREEARC_ERRCODE_GENERAL);
}

#include "CUI.h"
CUI UI;
FILE *g_logfile = NULL;

int main (int argc, char *argv[])
{
  COMMAND command (argc, argv);    // Parse command first
  if (!command.to_stdout && !command.silent)
    UI.DisplayHeader (HEADER1 NAME);
  if (command.ok)                  // If parsing succeeded
    PROCESS (&command, &UI);       // Execute command
  if (!command.to_stdout && !command.silent)
    cui_printf ("\n");
  if (g_logfile) {
    fclose(g_logfile);
    g_logfile = NULL;
  }
  return command.ok? EXIT_SUCCESS : FREEARC_EXIT_ERROR;
}

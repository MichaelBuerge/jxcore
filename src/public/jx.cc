// Copyright & License details are available under JXCORE_LICENSE file

#include "jx.h"
#include "../jxcore.h"
#include <stdio.h>
#include <string.h>

jxcore::JXEngine *engine = NULL;
JX_CALLBACK jx_callback;
char *argv = NULL;
char *app_args[2];

// allocates one extra JXResult memory at the end of the array
// Uses that one for a return value
#define CONVERT_ARG_TO_RESULT(results, context)                   \
  JXResult *results = NULL;                                       \
  const int len = args.Length() - start_arg;                      \
  {                                                               \
    results = (JXResult *)malloc(sizeof(JXResult) * (len + 1));   \
    for (int i = 0; i < len; i++) {                               \
      JS_HANDLE_VALUE val = args.GetItem(i - start_arg);          \
      results[i].context_ = context;                              \
      results[i].data_ = NULL;                                    \
      results[i].size_ = 0;                                       \
      results[i].type_ = RT_Undefined;                            \
      jxcore::JXEngine::ConvertToJXResult(com, val, &results[i]); \
    }                                                             \
    results[len].context_ = context;                              \
    results[len].data_ = NULL;                                    \
    results[len].size_ = 0;                                       \
    results[len].type_ = RT_Undefined;                            \
  }

JS_LOCAL_METHOD(asyncCallback) {
  const int start_arg = 0;
  CONVERT_ARG_TO_RESULT(results, __contextORisolate);
  jx_callback(results, len);
  free(results);
}
JS_METHOD_END

static int extension_id = 0;
#define MAX_CALLBACK_ID 1024
static JX_CALLBACK callbacks[MAX_CALLBACK_ID] = {0};

JS_LOCAL_METHOD(extensionCallback) {
  if (args.Length() == 0 || !args.IsUnsigned(0)) {
    THROW_EXCEPTION("This method expects the first parameter is unsigned");
  }

  const int interface_id = args.GetInteger(0);
  if (interface_id >= MAX_CALLBACK_ID || callbacks[interface_id] == NULL)
    THROW_EXCEPTION("There is no extension method for given Id");

  const int start_arg = 1;
  CONVERT_ARG_TO_RESULT(results, __contextORisolate);

  callbacks[interface_id](results, len);
  if (results[len].type_ != RT_Undefined) {
    if (results[len].type_ == RT_Error) {
      std::string msg = JX_GetString(&results[len]);
      JX_FreeResultData(&results[len]);
      THROW_EXCEPTION(msg.c_str());
    }
    JS_HANDLE_VALUE ret_val =
        jxcore::JXEngine::ConvertFromJXResult(com, &results[len]);
    JX_FreeResultData(&results[len]);
    RETURN_PARAM(ret_val);
  }
}
JS_METHOD_END

void JX_DefineExtension(const char *name, JX_CALLBACK callback) {
  int id = extension_id++;
  assert(id < MAX_CALLBACK_ID &&
         "Maximum amount of extension methods reached.");
  callbacks[id] = callback;
  engine->DefineProxyMethod(name, id, extensionCallback);
}

void JX_Initialize(const char *home_folder, JX_CALLBACK callback) {
  jx_callback = callback;

#if defined(__IOS__) || defined(__ANDROID__) || defined(DEBUG)
  warn_console("Initializing JXcore engine\n");
#endif

  if (engine != NULL) {
    warn_console("Destroying the previous JXcore engine instance\n");
    engine->Destroy();
    free(argv);
    delete engine;
  }

  size_t home_length = strlen(home_folder);
  argv = (char *)malloc((12 + home_length) * sizeof(char));
  memcpy(argv, home_folder, home_length * sizeof(char));
  memcpy(argv + home_length, "/jx\0main.js", 11 * sizeof(char));
  argv[home_length + 11] = '\0';

  app_args[0] = argv;
  app_args[1] = argv + home_length + 4;

  engine = new jxcore::JXEngine(2, app_args, false);
  engine->Init();

#if defined(__IOS__) || defined(__ANDROID__) || defined(DEBUG)
  warn_console("JXcore engine is ready\n");
#endif
}

bool JX_Evaluate(const char *data, const char *script_name,
                 JXResult *jxresult) {
  char *str;
  if (engine == NULL) {
    error_console("JXcore engine is not ready yet! (jx.cc:Evaluate)\n");
    str = strdup("undefined");
    return str;
  }

  const char *name = script_name == NULL ? "JX_Evaluate" : script_name;

  return engine->Evaluate(data, name, jxresult);
}

void JX_DefineMainFile(const char *data) {
  if (engine == NULL) {
    error_console("JXcore engine is not ready yet! (jx.cc:DefineMainFile)\n");
    return;
  }

  engine->MemoryMap("main.js", data, true);
}

void JX_DefineFile(const char *name, const char *file) {
  if (engine == NULL) {
    error_console("JXcore engine is not ready yet! (jx.cc:DefineFile)\n");
    return;
  }

  engine->MemoryMap(name, file);
}

void JX_StartEngine() {
  if (engine == NULL) {
    error_console("JXcore engine is not ready yet! (jx.cc:StartEngine)\n");
    return;
  }

  engine->DefineNativeMethod("asyncCallback", asyncCallback);

#if defined(__IOS__) || defined(__ANDROID__) || defined(DEBUG)
  warn_console("Starting JXcore engine\n");
#endif

  engine->Start();
  engine->LoopOnce();
#if defined(__IOS__) || defined(__ANDROID__) || defined(DEBUG)
  warn_console("JXcore engine is started\n");
#endif
}

int JX_LoopOnce() {
  if (engine == NULL) {
    error_console("JXcore engine is not ready yet! (jx.cc:LoopOnce)\n");
    return 0;
  }
  return engine->LoopOnce();
}

int JX_Loop() {
  if (engine == NULL) {
    error_console("JXcore engine is not ready yet! (jx.cc:Loop)\n");
    return 0;
  }
  return engine->Loop();
}

bool JX_IsV8() {
#ifdef JS_ENGINE_V8
  return true;
#else
  return false;
#endif
}

bool JX_IsSpiderMonkey() {
#ifdef JS_ENGINE_MOZJS
  return true;
#else
  return false;
#endif
}

void JX_StopEngine() {
  if (engine == NULL) {
    error_console("JXcore engine is not ready yet! (jx.cc:StopEngine)\n");
    return;
  }

#if defined(__IOS__) || defined(__ANDROID__) || defined(DEBUG)
  warn_console("Destroying JXcore engine\n");
#endif

  if (engine->LoopOnce()) {
    warn_console("JXcore engine event loop was still handling other events\n");
  }

  engine->Destroy();
  delete engine;
  engine = NULL;
#if defined(__IOS__) || defined(__ANDROID__) || defined(DEBUG)
  warn_console("JXcore engine is destroyed");
#endif
}

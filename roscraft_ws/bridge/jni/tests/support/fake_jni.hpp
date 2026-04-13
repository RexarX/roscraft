#pragma once

#include <jni.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace roscraft::bridge::jni::tests {

class FakeJniEnv {
public:
  FakeJniEnv() {
    env_.functions = &functions_;
    Registry()[&functions_] = this;

    functions_.NewGlobalRef = &NewGlobalRefThunk;
    functions_.DeleteGlobalRef = &DeleteGlobalRefThunk;
    functions_.DeleteLocalRef = &DeleteLocalRefThunk;
    functions_.GetObjectClass = &GetObjectClassThunk;
    functions_.GetMethodID = &GetMethodIDThunk;
    functions_.CallVoidMethod = &CallVoidMethodThunk;
    functions_.CallVoidMethodV = &CallVoidMethodVThunk;
    functions_.NewByteArray = &NewByteArrayThunk;
    functions_.SetByteArrayRegion = &SetByteArrayRegionThunk;
    functions_.ExceptionCheck = &ExceptionCheckThunk;
    functions_.ExceptionDescribe = &ExceptionDescribeThunk;
    functions_.ExceptionClear = &ExceptionClearThunk;
    functions_.GetJavaVM = &GetJavaVMThunk;
    functions_.GetArrayLength = &GetArrayLengthThunk;
    functions_.GetByteArrayElements = &GetByteArrayElementsThunk;
    functions_.ReleaseByteArrayElements = &ReleaseByteArrayElementsThunk;
  }

  FakeJniEnv(const FakeJniEnv&) = delete;
  FakeJniEnv(FakeJniEnv&&) = delete;
  ~FakeJniEnv() { Registry().erase(&functions_); }

  FakeJniEnv& operator=(const FakeJniEnv&) = delete;
  FakeJniEnv& operator=(FakeJniEnv&&) = delete;

  [[nodiscard]] JNIEnv* Env() noexcept {
    return reinterpret_cast<JNIEnv*>(&env_);
  }

  [[nodiscard]] const JNIEnv* Env() const noexcept {
    return reinterpret_cast<const JNIEnv*>(&env_);
  }

  [[nodiscard]] jbyteArray MakeByteArray(std::span<const uint8_t> bytes) {
    const auto handle = NewArrayHandle(static_cast<jsize>(bytes.size()));
    auto& storage = byte_arrays_[handle];
    for (size_t i = 0; i < bytes.size(); ++i) {
      storage[i] = static_cast<jbyte>(bytes[i]);
    }
    return handle;
  }

  [[nodiscard]] std::vector<uint8_t> CopyByteArray(jbyteArray array) const {
    const auto it = byte_arrays_.find(array);
    if (it == byte_arrays_.end()) {
      return {};
    }

    std::vector<uint8_t> out;
    out.reserve(it->second.size());
    for (const jbyte value : it->second) {
      out.push_back(static_cast<uint8_t>(value));
    }
    return out;
  }

  jobject callback_object =
      reinterpret_cast<jobject>(static_cast<uintptr_t>(0x101));
  jclass callback_class =
      reinterpret_cast<jclass>(static_cast<uintptr_t>(0x102));
  jmethodID on_packet_method =
      reinterpret_cast<jmethodID>(static_cast<uintptr_t>(0x103));

  JavaVM* java_vm = nullptr;
  jint get_java_vm_result = JNI_OK;

  bool fail_new_global_ref = false;
  bool fail_get_object_class = false;
  bool fail_get_method_id = false;
  bool fail_new_byte_array = false;
  bool fail_get_byte_array_elements = false;
  bool exception_pending = false;

  int new_global_ref_calls = 0;
  int delete_global_ref_calls = 0;
  int delete_local_ref_calls = 0;
  int get_object_class_calls = 0;
  int get_method_id_calls = 0;
  int call_void_method_calls = 0;
  int new_byte_array_calls = 0;
  int set_byte_array_region_calls = 0;
  int exception_check_calls = 0;
  int exception_describe_calls = 0;
  int exception_clear_calls = 0;
  int get_java_vm_calls = 0;
  int get_array_length_calls = 0;
  int get_byte_array_elements_calls = 0;
  int release_byte_array_elements_calls = 0;

  std::string last_method_name;
  std::string last_method_signature;
  jobject last_call_object = nullptr;
  jmethodID last_call_method = nullptr;
  jbyteArray last_call_packet = nullptr;
  jint last_release_mode = 0;

  std::vector<jobject> deleted_global_refs;
  std::vector<jobject> deleted_local_refs;
  std::vector<std::vector<uint8_t>> callback_packets;

private:
  [[nodiscard]] static auto Registry()
      -> std::unordered_map<const JNINativeInterface_*, FakeJniEnv*>& {
    static std::unordered_map<const JNINativeInterface_*, FakeJniEnv*> registry;
    return registry;
  }

  [[nodiscard]] static FakeJniEnv& Self(JNIEnv* env) {
    auto* native = reinterpret_cast<JNIEnv_*>(env);
    return *Registry().at(native->functions);
  }

  [[nodiscard]] jbyteArray NewArrayHandle(jsize len) {
    next_handle_ += 0x10;
    const auto handle = reinterpret_cast<jbyteArray>(next_handle_);
    byte_arrays_.emplace(handle, std::vector<jbyte>(static_cast<size_t>(len)));
    return handle;
  }

  static jobject JNICALL NewGlobalRefThunk(JNIEnv* env, jobject obj) {
    auto& self = Self(env);
    ++self.new_global_ref_calls;
    if (self.fail_new_global_ref) {
      return nullptr;
    }
    return obj;
  }

  static void JNICALL DeleteGlobalRefThunk(JNIEnv* env, jobject gref) {
    auto& self = Self(env);
    ++self.delete_global_ref_calls;
    self.deleted_global_refs.push_back(gref);
  }

  static void JNICALL DeleteLocalRefThunk(JNIEnv* env, jobject obj) {
    auto& self = Self(env);
    ++self.delete_local_ref_calls;
    self.deleted_local_refs.push_back(obj);
  }

  static jclass JNICALL GetObjectClassThunk(JNIEnv* env, jobject /*obj*/) {
    auto& self = Self(env);
    ++self.get_object_class_calls;
    if (self.fail_get_object_class) {
      return nullptr;
    }
    return self.callback_class;
  }

  static jmethodID JNICALL GetMethodIDThunk(JNIEnv* env, jclass /*clazz*/,
                                            const char* name,
                                            const char* signature) {
    auto& self = Self(env);
    ++self.get_method_id_calls;
    self.last_method_name = name != nullptr ? name : "";
    self.last_method_signature = signature != nullptr ? signature : "";
    if (self.fail_get_method_id) {
      return nullptr;
    }
    return self.on_packet_method;
  }

  static void JNICALL CallVoidMethodThunk(JNIEnv* env, jobject obj,
                                          jmethodID method_id, ...) {
    va_list args;
    va_start(args, method_id);
    CallVoidMethodImpl(Self(env), obj, method_id, args);
    va_end(args);
  }

  static void JNICALL CallVoidMethodVThunk(JNIEnv* env, jobject obj,
                                           jmethodID method_id, va_list args) {
    CallVoidMethodImpl(Self(env), obj, method_id, args);
  }

  static void CallVoidMethodImpl(FakeJniEnv& self_ref, jobject obj,
                                 jmethodID method_id, va_list args) {
    ++self_ref.call_void_method_calls;
    self_ref.last_call_object = obj;
    self_ref.last_call_method = method_id;
    const auto packet = va_arg(args, jbyteArray);

    self_ref.last_call_packet = packet;
    if (packet != nullptr) {
      self_ref.callback_packets.push_back(self_ref.CopyByteArray(packet));
    }
  }

  static jbyteArray JNICALL NewByteArrayThunk(JNIEnv* env, jsize len) {
    auto& self = Self(env);
    ++self.new_byte_array_calls;
    if (self.fail_new_byte_array || len < 0) {
      return nullptr;
    }
    return self.NewArrayHandle(len);
  }

  static void JNICALL SetByteArrayRegionThunk(JNIEnv* env, jbyteArray array,
                                              jsize start, jsize len,
                                              const jbyte* buf) {
    auto& self = Self(env);
    ++self.set_byte_array_region_calls;

    if (array == nullptr || buf == nullptr || start < 0 || len < 0) {
      return;
    }

    const auto it = self.byte_arrays_.find(array);
    if (it == self.byte_arrays_.end()) {
      return;
    }

    const size_t begin = static_cast<size_t>(start);
    const size_t count = static_cast<size_t>(len);
    if (begin + count > it->second.size()) {
      return;
    }

    for (size_t i = 0; i < count; ++i) {
      it->second[begin + i] = buf[i];
    }
  }

  static jboolean JNICALL ExceptionCheckThunk(JNIEnv* env) {
    auto& self = Self(env);
    ++self.exception_check_calls;
    return self.exception_pending ? JNI_TRUE : JNI_FALSE;
  }

  static void JNICALL ExceptionDescribeThunk(JNIEnv* env) {
    auto& self = Self(env);
    ++self.exception_describe_calls;
  }

  static void JNICALL ExceptionClearThunk(JNIEnv* env) {
    auto& self = Self(env);
    ++self.exception_clear_calls;
    self.exception_pending = false;
  }

  static jint JNICALL GetJavaVMThunk(JNIEnv* env, JavaVM** vm) {
    auto& self = Self(env);
    ++self.get_java_vm_calls;
    if (vm != nullptr) {
      *vm = self.java_vm;
    }
    return self.get_java_vm_result;
  }

  static jsize JNICALL GetArrayLengthThunk(JNIEnv* env, jarray array) {
    auto& self = Self(env);
    ++self.get_array_length_calls;

    const auto it = self.byte_arrays_.find(reinterpret_cast<jbyteArray>(array));
    if (it == self.byte_arrays_.end()) {
      return 0;
    }
    return static_cast<jsize>(it->second.size());
  }

  static jbyte* JNICALL GetByteArrayElementsThunk(JNIEnv* env, jbyteArray array,
                                                  jboolean* is_copy) {
    auto& self = Self(env);
    ++self.get_byte_array_elements_calls;

    if (self.fail_get_byte_array_elements) {
      return nullptr;
    }

    const auto it = self.byte_arrays_.find(array);
    if (it == self.byte_arrays_.end()) {
      return nullptr;
    }

    if (is_copy != nullptr) {
      *is_copy = JNI_TRUE;
    }
    return it->second.data();
  }

  static void JNICALL ReleaseByteArrayElementsThunk(JNIEnv* env,
                                                    jbyteArray /*array*/,
                                                    jbyte* /*elems*/,
                                                    jint mode) {
    auto& self = Self(env);
    ++self.release_byte_array_elements_calls;
    self.last_release_mode = mode;
  }

  JNIEnv_ env_{};
  JNINativeInterface_ functions_{};

  uintptr_t next_handle_ = 0x1000;
  std::unordered_map<jbyteArray, std::vector<jbyte>> byte_arrays_;
};

class FakeJavaVM {
public:
  FakeJavaVM() {
    vm_.functions = &functions_;
    Registry()[&functions_] = this;

    functions_.DestroyJavaVM = &DestroyJavaVMThunk;
    functions_.GetEnv = &GetEnvThunk;
    functions_.AttachCurrentThread = &AttachCurrentThreadThunk;
    functions_.DetachCurrentThread = &DetachCurrentThreadThunk;
    functions_.AttachCurrentThreadAsDaemon = &AttachCurrentThreadAsDaemonThunk;
  }

  FakeJavaVM(const FakeJavaVM&) = delete;
  FakeJavaVM(FakeJavaVM&&) = delete;

  ~FakeJavaVM() { Registry().erase(&functions_); }

  FakeJavaVM& operator=(const FakeJavaVM&) = delete;
  FakeJavaVM& operator=(FakeJavaVM&&) = delete;

  [[nodiscard]] JavaVM* Vm() noexcept {
    return reinterpret_cast<JavaVM*>(&vm_);
  }

  void Bind(FakeJniEnv& env) {
    env_ptr = env.Env();
    env.java_vm = Vm();
  }

  JNIEnv* env_ptr = nullptr;

  jint get_env_result = JNI_OK;
  jint attach_result = JNI_OK;
  jint detach_result = JNI_OK;
  jint attach_daemon_result = JNI_OK;

  int destroy_calls = 0;
  int get_env_calls = 0;
  int attach_calls = 0;
  int detach_calls = 0;
  int attach_daemon_calls = 0;

  jint last_get_env_version = 0;
  jint last_attach_version = 0;
  std::string last_attach_name;
  jobject last_attach_group = nullptr;

private:
  [[nodiscard]] static auto Registry()
      -> std::unordered_map<const JNIInvokeInterface_*, FakeJavaVM*>& {
    static std::unordered_map<const JNIInvokeInterface_*, FakeJavaVM*> registry;
    return registry;
  }

  [[nodiscard]] static FakeJavaVM& Self(JavaVM* vm) {
    auto* native = reinterpret_cast<JavaVM_*>(vm);
    return *Registry().at(native->functions);
  }

  static jint JNICALL DestroyJavaVMThunk(JavaVM* vm) {
    auto& self = Self(vm);
    ++self.destroy_calls;
    return JNI_OK;
  }

  static jint JNICALL GetEnvThunk(JavaVM* vm, void** penv, jint version) {
    auto& self = Self(vm);
    ++self.get_env_calls;
    self.last_get_env_version = version;

    if (penv != nullptr) {
      *penv = self.get_env_result == JNI_OK ? self.env_ptr : nullptr;
    }
    return self.get_env_result;
  }

  static jint JNICALL AttachCurrentThreadThunk(JavaVM* vm, void** penv,
                                               void* args) {
    auto& self = Self(vm);
    ++self.attach_calls;

    if (args != nullptr) {
      const auto* attach_args = static_cast<const JavaVMAttachArgs*>(args);
      self.last_attach_version = attach_args->version;
      self.last_attach_name =
          attach_args->name != nullptr ? attach_args->name : "";
      self.last_attach_group = attach_args->group;
    }

    if (penv != nullptr) {
      *penv = self.attach_result == JNI_OK ? self.env_ptr : nullptr;
    }
    return self.attach_result;
  }

  static jint JNICALL DetachCurrentThreadThunk(JavaVM* vm) {
    auto& self = Self(vm);
    ++self.detach_calls;
    return self.detach_result;
  }

  static jint JNICALL AttachCurrentThreadAsDaemonThunk(JavaVM* vm, void** penv,
                                                       void* /*args*/) {
    auto& self = Self(vm);
    ++self.attach_daemon_calls;
    if (penv != nullptr) {
      *penv = self.attach_daemon_result == JNI_OK ? self.env_ptr : nullptr;
    }
    return self.attach_daemon_result;
  }

  JavaVM_ vm_{};
  JNIInvokeInterface_ functions_{};
};

}  // namespace roscraft::bridge::jni::tests

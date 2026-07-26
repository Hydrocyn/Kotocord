#ifndef RAII_H
#define RAII_H

// ==========================================
// RAII.h — C 资源封装与智能指针自定义删除器
// ==========================================
// 本项目同时使用 Qt 对象树 (QObject parent-child) 和 C API (Vosk, Whisper)。
// Qt 部分保持惯用法（new QObject(this)），C 部分则用 unique_ptr + 自定义删除器
// 封装，实现 RAII 自动化资源管理。
//
// 设计依据: C++ Core Guidelines
//   - R.1: 通过资源句柄和 RAII 自动管理资源
//   - F.7: 非拥有指针用裸指针，拥有型用智能指针
//   - I.11: 永不通过裸指针传递所有权
// ==========================================

#include <memory>

// 前向声明第三方 C API 类型（避免在不需要 Vosk/Whisper 的地方拖入大个头文件）
struct VoskModel;
struct VoskRecognizer;
struct whisper_context;

// 实际释放函数声明（在对应的 .cpp 中由 vosk_api.h / whisper.h 提供）
extern "C" {
    void vosk_model_free(struct VoskModel* model);
    void vosk_recognizer_free(struct VoskRecognizer* recognizer);
    void whisper_free(struct whisper_context* ctx);
}

// ==========================================
// 1. VoskModel 删除器
// ==========================================
// 用法: UniqueVoskModel model( vosk_model_new(...) );
//       离开作用域时自动调用 vosk_model_free，无需手动释放。
struct VoskModelDeleter {
    void operator()(VoskModel* p) const noexcept {
        if (p) vosk_model_free(p);
    }
};

/// 拥有 VoskModel 所有权的 unique_ptr 别名
using UniqueVoskModel = std::unique_ptr<VoskModel, VoskModelDeleter>;

// ==========================================
// 2. VoskRecognizer 删除器
// ==========================================
// 注意: VoskRecognizer 生命周期应短于其归属的 VoskModel（识别器依赖模型）。
// unique_ptr 的成员声明顺序（先 recognizer 后 model）保证了析构顺序为 LIFO，
// 即 recognizer 先析构，model 后析构——这与依赖关系一致。
struct VoskRecognizerDeleter {
    void operator()(VoskRecognizer* p) const noexcept {
        if (p) vosk_recognizer_free(p);
    }
};

/// 拥有 VoskRecognizer 所有权的 unique_ptr 别名
using UniqueVoskRecognizer = std::unique_ptr<VoskRecognizer, VoskRecognizerDeleter>;

// ==========================================
// 3. Whisper 上下文删除器
// ==========================================
// Whisper 上下文可能占用数百 MB 显存/内存。使用 unique_ptr 确保:
//   - 构造失败（model 加载到一半抛异常）时，已分配的资源被回滚
//   - 析构顺序由编译器保证，与成员声明顺序相反
struct WhisperContextDeleter {
    void operator()(whisper_context* p) const noexcept {
        if (p) whisper_free(p);
    }
};

/// 拥有 whisper_context 所有权的 unique_ptr 别名
using UniqueWhisperContext = std::unique_ptr<whisper_context, WhisperContextDeleter>;

// ==========================================
// 4. Qt 侧说明
// ==========================================
// 以下场景不适用 unique_ptr，保持裸指针：
//
//   a) QObject 子对象 — new QObject(this) 已由 Qt 父子树管理
//      例如: QNetworkAccessManager* m = new QNetworkAccessManager(this);
//
//   b) 依赖注入 — 非拥有观察，生命周期由外部管理
//      例如: ILanguageModel* m_llm;  // AppController 不负责释放
//
//   c) Qt API 返回的内部指针 — QAudioSource::start() 返回 QIODevice*
//      该指针归 QAudioSource 内部所有，调用者不释放

#endif // RAII_H

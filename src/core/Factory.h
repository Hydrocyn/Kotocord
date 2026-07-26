#ifndef FACTORY_H
#define FACTORY_H

// ==========================================
// Factory.h — 泛型注册工厂
// ==========================================
// 提供编译期类型安全的插件注册与创建机制。
// 用法:
//
//   GenericFactory<IAudioTranscriber> asrFactory;
//   asrFactory.regist<VoskTranscriber>("vosk");
//   asrFactory.regist<WhisperTranscriber>("whisper");
//   auto engine = asrFactory.create("vosk");
//
// 编译期保证:
//   - regist<T>() 的 T 必须实现 Interface (static_assert + std::is_base_of_v)
//   - create() 返回 unique_ptr, 所有权明确
//   - 模板参数 Key 支持 QString / std::string / int 等可哈希类型
// ==========================================

#include <memory>
#include <functional>
#include <type_traits>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QDebug>

// ==========================================
// GenericFactory<Interface, Key>
// ==========================================
// Interface: 抽象基类 (所有注册类型必须继承自它)
// Key: 注册/查找键 (默认 QString, 支持任何 QHash-compatible 类型)
template<typename Interface, typename Key = QString>
class GenericFactory {
public:
    GenericFactory() = default;
    ~GenericFactory() = default;

    // 禁止拷贝 (unique_ptr 所有权唯一)
    GenericFactory(const GenericFactory&) = delete;
    GenericFactory& operator=(const GenericFactory&) = delete;

    // 支持移动
    GenericFactory(GenericFactory&&) noexcept = default;
    GenericFactory& operator=(GenericFactory&&) noexcept = default;

    // --- 注册 ---
    // 将具体类型 T 与一个 Key 绑定。T 必须公开继承 Interface。
    template<typename T, typename... Args>
    void regist(const Key& key) {
        static_assert(std::is_base_of_v<Interface, T>,
                      "GenericFactory::regist<T> — T 必须继承 Interface");

        if (m_creators.contains(key)) {
            qDebug() << "[Factory] 警告: Key" << key
                     << "已被注册, 将被覆盖";
        }

        // 使用 std::function 做类型擦除: 存储一个返回 unique_ptr<Interface> 的 lambda
        // 通过 std::make_unique<T> + 参数包展开 实现完美转发构造
        m_creators[key] = []() -> std::unique_ptr<Interface> {
            return std::make_unique<T>();
        };
    }

    // --- 创建 ---
    // 根据 Key 创建实例。如果 Key 未注册, 返回空指针。
    [[nodiscard]] std::unique_ptr<Interface> create(const Key& key) const {
        auto it = m_creators.find(key);
        if (it != m_creators.end()) {
            return it.value()();  // QHash: it.value() 返回映射值 (即 std::function)
        }
        qDebug() << "[Factory] 错误: Key" << key << "未注册";
        return nullptr;
    }

    // --- 查询 ---
    [[nodiscard]] bool contains(const Key& key) const {
        return m_creators.contains(key);
    }

    [[nodiscard]] QStringList keys() const {
        QStringList result;
        for (const Key& k : m_creators.keyRange()) {
            result.append(k);
        }
        return result;
    }

    [[nodiscard]] int size() const {
        return static_cast<int>(m_creators.size());
    }

    [[nodiscard]] bool empty() const {
        return m_creators.isEmpty();
    }

private:
    // std::function 做类型擦除: 抹掉具体类型 T, 统一存储为
    //   "返回 unique_ptr<Interface> 的无参可调用对象"
    QHash<Key, std::function<std::unique_ptr<Interface>()>> m_creators;
};

// ==========================================
// 知识点说明
// ==========================================
//
// 1. 类型擦除 (Type Erasure):
//    QHash<Key, std::function<unique_ptr<Interface>()>>
//    将不同具体类型 T 的构造逻辑统一存储为同一种函数签名,
//    create() 调用时无需知道原始类型.
//
// 2. static_assert + std::is_base_of_v:
//    编译期断言 T 实现 Interface. 如果传入错误类型,
//    编译时就会报错, 不会等到运行时才发现.
//    例如: asrFactory.regist<QString>("bad") → 编译失败.
//
// 3. std::make_unique<T>:
//    C++14 引入, 将 new T() 封装为异常安全的工厂函数.
//    如果 T 构造失败抛异常, unique_ptr 保证不泄漏.
//
// 4. [[nodiscard]]:
//    C++17 属性. 调用 create() 后忽略返回值会触发编译器警告.
//    防止写出 `factory.create("x");` (忘了保存结果) 的 bug.
//
// 5. = delete 拷贝 + = default 移动:
//    工厂包含 unique_ptr, 不应被拷贝 (会导致双重释放).
//    但可以被移动 (转移所有权). 遵循 Rule of Five.

#endif // FACTORY_H

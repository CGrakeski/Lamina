#include "lmStruct.hpp"
#include "../../interpreter/lamina_api/lamina.hpp"
#include "standard.hpp"
#include <algorithm>

lmStruct::lmStruct() {
    constexpr size_t init_size = 16;
    buckets_.resize(init_size, nullptr);
}

// 初始化lStruct
lmStruct::lmStruct(const std::vector<std::pair<std::string, Value>>& vec) {
    // 初始化桶数组
    size_t init_size = 16;
    while (init_size < (vec.size() / load_factor_)) {
        init_size *= 2;
    }
    buckets_.resize(init_size, nullptr);

    // 插入所有初始键值对
    for (auto [name, val] : vec) {
        insert(name, val);
    }
}


lmStruct::~lmStruct() = default;

// 查找键对应的节点（返回nullptr表示未找到）
std::shared_ptr<Node> lmStruct::find(const std::string& key) const {
    if (buckets_.empty()) return nullptr;   // 桶数组未初始化时直接返回

    const size_t hash = hash_string(key);
    const size_t hash_parent = hash_string("__parent__");
    const size_t bucket_idx = getBucketIndex(hash);
    Value parent;

    // 遍历桶内链表查找（先比哈希，再比字符串）
    std::shared_ptr<Node> current = buckets_[bucket_idx];
    while (current != nullptr) {
        if (current->hash == hash && current->key == key) {
            return current; // 找到匹配节点
        }
        if (current->hash == hash_parent && current->key == "__parent__") {
            parent = current->value;
        }
        current = current->next;
    }
    if (!parent.is_lstruct()) return nullptr;
    if (
        auto p = std::get<std::shared_ptr<lmStruct>>(parent.data);
        p != nullptr
    ){
        return p->find(key);
    }

    return nullptr; // 未找到
}

// 插入或更新键值对
Value lmStruct::insert(const std::string& key, Value val) {
    if (buckets_.empty()) {
        buckets_.resize(16, nullptr);   // 初始化为16个桶
    }

    // 检查负载因子
    if (static_cast<float>(elem_count_) / buckets_.size() >= load_factor_) {
        this->resize();
    }

    const size_t hash = hash_string(key);
    const size_t bucket_idx = getBucketIndex(hash);

    // 是否已存在
    std::shared_ptr<Node> current = buckets_[bucket_idx];
    while (current != nullptr) {
        if (current->hash == hash && current->key == key) {
            current->value = std::move(val);
            return LAMINA_NULL;
        }
        current = current->next;
    }

    // 不存在则创建新节点
    const auto new_node = std::make_shared<Node>(key, std::move(val));
    new_node->next = buckets_[bucket_idx];  // 新节点的next指向原头节点
    buckets_[bucket_idx] = new_node;
    elem_count_++;
    return LAMINA_NULL;
}

std::vector<std::pair<std::string, Value>> lmStruct::to_vector() const {
    std::vector<std::pair<std::string, Value>> vec;
    for (const std::shared_ptr<Node>& bucket_head : buckets_) {
        auto current = bucket_head;
        while (current != nullptr) {
            vec.emplace_back(current->key, current->value);
            current = current->next;
        }
    }
    return vec;
}


std::string lmStruct::to_string() const {
    std::stringstream ss;
    ss << "{ ";

    // 统计总节点数
    size_t total_node_count = 0;
    for (const std::shared_ptr<Node>& bucket_head : buckets_) {
        auto current = bucket_head;
        while (current != nullptr) {
            total_node_count++;
            current = current->next;
        }
    }

    // 添加键值对
    size_t current_node_idx = 0;  // 记录当前节点
    for (const std::shared_ptr<Node>& bucket_head : buckets_) {
        auto current = bucket_head;
        while (current != nullptr) {
            // 写入当前键值对
            ss << current->key << ": " << current->value.to_string();

            if (current_node_idx < total_node_count - 1) {
                ss << ", ";
            }

            current = current->next;
            current_node_idx++;  // 更新节点索引
        }
    }

    ss << " }";
    return ss.str();  // 转换为string返回
}


Value new_lm_struct(const std::vector<std::pair<std::string, Value>>& vec) {
    const auto lstruct_ptr = std::make_shared<lmStruct>(vec);
    return Value(lstruct_ptr);
}

Value getattr(const std::vector<Value>& args) {
    const auto& lstruct_ = std::get<std::shared_ptr<lmStruct>>(args[0].data);
    const auto& attr_name = std::get<std::string>(args[1].data);
    auto res = lstruct_->find(attr_name);
    if (res == nullptr) {
        L_ERR("AttrError: struct hasn't attribute named " + attr_name);
        return LAMINA_NULL;
    }
    return res->value;
}

Value setattr(const std::vector<Value>& args) {
    std::shared_ptr<lmStruct> lstruct_ = std::get<std::shared_ptr<lmStruct>>(args[0].data);
    const auto& attr_name = std::get<std::string>(args[1].data);
    Value value = args[2];
    lstruct_->insert(attr_name, value);
    return LAMINA_NULL;
}

Value update(const std::vector<Value>& args) {
    std::shared_ptr<lmStruct> lstruct_a = std::get<std::shared_ptr<lmStruct>>(args[0].data);
    const auto& lstruct_b = std::get<std::shared_ptr<lmStruct>>(args[1].data);
    auto vec = lstruct_b->to_vector();
    for (auto& [key, val]: vec) {
        lstruct_a->insert(key, val);
    }
    return LAMINA_NULL;
}

Value copy_struct(const std::vector<Value>& args){
    if (args.empty()) return LAMINA_NULL;
    const auto& arg_data = args[0].data;
    auto& original_ptr = std::get<std::shared_ptr<lmStruct>>(arg_data);

    if (!original_ptr) return LAMINA_NULL;

    const lmStruct& original_obj = *original_ptr;
    // 复制对象
    const auto new_obj = std::make_shared<lmStruct>(original_obj);
    return new_obj;
}

std::string lStruct_to_string(const std::shared_ptr<lmStruct>& lstruct) {
    if (lstruct == nullptr) {
        return "{}";
    }
    return lstruct->to_string();
}

Value new_struct_from(const std::vector<Value>& args) {
    check_cpp_function_argv(args, {Value::Type::lmStruct});
    auto arg_data = args[0].data;
    auto original_ptr = std::get<std::shared_ptr<lmStruct>>(arg_data);

    if (!original_ptr) return LAMINA_NULL;

    // 新对象
    const auto new_obj = std::make_shared<lmStruct>();
    new_obj->insert("__parent__", Value(original_ptr));
    return {new_obj};

}

Value is_same_thing(const std::vector<Value>& args) {
    check_cpp_function_argv(args,
    {Value::Type::lmStruct, Value::Type::lmStruct}
    );
    const auto arg_0 = std::get<std::shared_ptr<lmStruct>>(args[0].data);
    const auto arg_1 = std::get<std::shared_ptr<lmStruct>>(args[1].data);
    if (arg_0.get() == arg_1.get()) {
        return LAMINA_BOOL(true);
    }
    return LAMINA_BOOL(false);
}
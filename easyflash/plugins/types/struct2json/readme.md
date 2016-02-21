# C结构体与 JSON 快速互转库

---

## struct2json

[struct2json](https://github.com/armink/struct2json) 是一个开源的C结构体与 JSON 快速互转库，它可以快速实现 **结构体对象** 与 **JSON 对象** 之间序列化及反序列化要求。快速、简洁的 API 设计，大大降低直接使用 JSON 解析库来实现此类功能的代码复杂度。

## 起源

把面向对象设计应用到C语言中，是当下很流行的设计思想。由于C语言中没有类，所以一般使用结构体 `struct` 充当类，那么结构体变量就是对象。有了对象之后，很多时候需要考虑对象的序列化及反序列化问题。C语言不像很多高级语言拥有反射等机制，使得对象序列化及反序列化被原生的支持。

对于C语言来说，序列化为 JSON 字符串是个不错的选择，所以就得使用 [cJSON](https://github.com/kbranigan/cJSON) 这类 JSON 解析库，但是使用后的代码冗余且逻辑性差，所以萌生对cJSON库进行二次封装，实现一个 struct 与 JSON 之间快速互转的库。 struct2json 就诞生于此。下面是 struct2json 主要使用场景：

- **持久化** ：结构体对象序列化为 JSON 对象后，可直接保存至文件、Flash，实现对结构体对象的掉电存储；
- **通信** ：高级语言对JSON支持的很友好，例如： Javascript、Groovy 就对 JSON 具有原生的支持，所以 JSON 也可作为C语言与其他语言软件之间的通信协议格式及对象传递格式；
- **可视化** ：序列化为 JSON 后的对象，可以更加直观的展示到控制台或者 UI 上，可用于产品调试、产品二次开发等场景；

## 如何使用

### 声明结构体

如下声明了两个结构体，结构体 `Hometown` 是结构体 `Student` 的子结构体

```C
/* 籍贯 */
typedef struct {
    char name[16];
} Hometown;

/* 学生 */
typedef struct {
    uint8_t id;
    uint8_t score[8];
    char name[10];
    double weight;
    Hometown hometown;
} Student;
```

### 将结构体对象序列化为 JSON 对象

|未使用（[源文件](https://github.com/armink/struct2json/blob/master/docs/zh/assets/not_use_struct2json.c)）|使用后（[源文件](https://github.com/armink/struct2json/blob/master/docs/zh/assets/used_struct2json.c)）|
|:-----:|:-----:|
|![结构体转JSON-使用前](https://git.oschina.net/Armink/struct2json/raw/master/docs/zh/images/not_use_struct2json.png)| ![结构体转JSON-使用后](https://git.oschina.net/Armink/struct2json/raw/master/docs/zh/images/used_struct2json.png)|

### 将 JSON 对象反序列化为结构体对象

|未使用（[源文件](https://github.com/armink/struct2json/blob/master/docs/zh/assets/not_use_struct2json_for_json.c)）|使用后（[源文件](https://github.com/armink/struct2json/blob/master/docs/zh/assets/used_struct2json_for_json.c)）|
|:-----:|:-----:|
|![JSON转结构体-使用前](https://git.oschina.net/Armink/struct2json/raw/master/docs/zh/images/not_use_struct2json_for_json.png)| ![JSON转结构体-使用后](https://git.oschina.net/Armink/struct2json/raw/master/docs/zh/images/used_struct2json_for_json.png)|

欢迎大家 **fork and pull request**([Github](https://github.com/armink/struct2json)|[OSChina](http://git.oschina.net/armink/struct2json)|[Coding](https://coding.net/u/armink/p/struct2json/git)) 。如果觉得这个开源项目很赞，可以点击[项目主页](https://github.com/armink/struct2json) 右上角的**Star**，同时把它推荐给更多有需要的朋友。

## 文档

具体内容参考[`\docs\zh\`](https://github.com/armink/struct2json/tree/master/docs/zh)下的文件。务必保证在 **阅读文档** 后再使用。

## 许可

MIT Copyright (c) armink.ztl@gmail.com

```C
cJSON *struct_to_json(void* struct_obj) {
    Student *struct_student = (Student *)struct_obj;

    /* 创建Student JSON对象 */
    s2j_create_json_obj(json_student);

    /* 序列化数据到Student JSON对象 */
    s2j_json_set_basic_element(json_student, struct_student, int, id);
    s2j_json_set_basic_element(json_student, struct_student, double, weight);
    s2j_json_set_array_element(json_student, struct_student, int, score, 8);
    s2j_json_set_basic_element(json_student, struct_student, string, name);

    /* 序列化数据到Student.Hometown JSON对象 */
    s2j_json_set_struct_element(json_hometown, json_student, struct_hometown, struct_student, Hometown, hometown);
    s2j_json_set_basic_element(json_hometown, struct_hometown, string, name);

    /* 返回Student JSON对象指针 */
    return json_student;
}











```
```C
cJSON *struct_to_json(void* struct_obj) {
    Student *struct_student = (Student *) struct_obj;
    cJSON *score, *score_element;
    size_t index = 0;

    /* 创建Student JSON对象 */
    cJSON *json_student = cJSON_CreateObject();

    /* 序列化数据到Student JSON对象 */
    cJSON_AddNumberToObject(json_student, "id", struct_student->id);
    cJSON_AddNumberToObject(json_student, "weight", struct_student->weight);
    score = cJSON_CreateArray();
    if (score) {
        while (index < 8) {
            score_element = cJSON_CreateNumber(struct_student->score[index++]);
            cJSON_AddItemToArray(score, score_element);
        }
        cJSON_AddItemToObject(json_student, "score", score);
    }
    cJSON_AddStringToObject(json_student, "name", struct_student->name);

    /* 序列化数据到Student.Hometown JSON对象 */
    Hometown *hometown_struct = &(struct_student->hometown);
    cJSON *hometown_json = cJSON_CreateObject();
    cJSON_AddItemToObject(json_student, "hometown", hometown_json);
    cJSON_AddStringToObject(hometown_json, "name", hometown_struct->name);

    /* 返回Student JSON对象指针 */
    return json_student;
}

```


```C
void *json_to_struct(cJSON* json_obj) {
    /* 创建Student结构体对象 */
    s2j_create_struct_obj(struct_student, Student);

    /* 反序列化数据到Student结构体对象 */
    s2j_struct_get_basic_element(struct_student, json_obj, int, id);
    s2j_struct_get_array_element(struct_student, json_obj, int, score);
    s2j_struct_get_basic_element(struct_student, json_obj, string, name);
    s2j_struct_get_basic_element(struct_student, json_obj, double, weight);

    /* 反序列化数据到Student.Hometown结构体对象 */
    s2j_struct_get_struct_element(struct_hometown, struct_student, json_hometown, json_obj, Hometown, hometown);
    s2j_struct_get_basic_element(struct_hometown, json_hometown, string, name);

    /* 返回Student结构体对象指针 */
    return struct_student;
}





























```

```
void *json_to_struct(cJSON* json_obj) {
    cJSON *json_temp, *score, *score_element;
    Student *struct_student;
    size_t index = 0, score_size = 0;

    /* 创建Student结构体对象 */
    struct_student = s2j_malloc(sizeof(Student));
    if (struct_student) {
        memset(struct_student, 0, sizeof(Student));
    }

    /* 反序列化数据到Student结构体对象 */
    json_temp = cJSON_GetObjectItem(json_obj, "id");
    if (json_temp) {
        struct_student->id = json_temp->valueint;
    }
    score = cJSON_GetObjectItem(json_obj, "score")
    if (score) {
        score_size = cJSON_GetArraySize(score);
        while (index < score_size) {
            score_element = cJSON_GetArrayItem(score, index);
            if (score_element) {
                struct_student->score[index++] = score_element->valueint;
            }
        }
    }
    json_temp = cJSON_GetObjectItem(json_obj, "name");
    if (json_temp) {
        strcpy(struct_student->name, json_temp->valuestring);
    }
    json_temp = cJSON_GetObjectItem(json_obj, "weight");
    if (json_temp) {
        struct_student->weight = json_temp->valuedouble;
    }

    /* 反序列化数据到Student.Hometown结构体对象 */
    Hometown *struct_hometown = &(struct_student->hometown);
    cJSON *json_hometown = cJSON_GetObjectItem(json_obj, "hometown");
    json_temp = cJSON_GetObjectItem(json_hometown, "name");
    if (json_temp) {
        strcpy(struct_hometown->name, json_temp->valuestring);
    }

    /* 返回Student结构体对象指针 */
    return struct_student;
}
```
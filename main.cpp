#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <ctime>
#include <random>
#include <pthread.h>

#include <string>
#include <vector>
#include <utility>

#include "kbhit.h"
#include "tool.h"

#define BLOCK_SIZE 4

void init();
void update();
void render();
void bye();

int main()
{
    init();

    for(;;)
    {
        update();
        render();
    }

    bye();

    return 0;
}

struct config
{
public:
    int width;
    int height;
    int* items;
    ~config()
    {
        delete[] items;
    }
};

// gm控制游戏的流程
class gameManager
{
public:
    // a: config.width * config.height
    // b: 4 * 4 写死的
    // c: 分数区域及其他信息

    config cfg;
    int score = 0;
    int m_class = 1;
    bool isGG = false;

    struct area_item
    {
        unsigned char data; // 颜色数据
        unsigned char status; // 0 固定块 1 活动快 2 预测块
    };

    std::vector<std::vector<area_item>> area_a;
    std::vector<std::vector<unsigned char>> area_b;

    bool isFalling = false;
    bool isInit = false;

    void init()
    {
        // 初始化a区缓冲区
        area_a.resize(cfg.height);
        for (int i = 0; i < area_a.size(); ++i)
            area_a[i].resize(cfg.width);
        // 初始化b区缓冲区
        area_b.resize(BLOCK_SIZE);
        for (int i = 0; i < area_b.size(); ++i)
            area_b[i].resize(BLOCK_SIZE);

    }

    void random4areaB()
    {
        std::default_random_engine e;
        std::uniform_int_distribution<int> u(0, 6); // 左闭右闭区间
        std::uniform_int_distribution<int> u2(1, 6); // 左闭右闭区间
        e.seed(time(0));

        int r_idx = u(e); // rand() % 7;
        int color = u2(e); // 颜色
        int item = cfg.items[r_idx];
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            for (int j = 0; j < BLOCK_SIZE; ++j)
            {
                area_b[i][j] = (item & (1 << (i * BLOCK_SIZE + j))) != 0 ? color : 0;
            }
        }
    }

    // 1 left 2 right 3 down
    void move(unsigned char type)
    {
        switch (type)
        {
        case 1: // left
        {
            bool isCanLeft = true;
            for (int i = 0; i < area_a.size(); ++i)
            {
                for (int j = 0; j < area_a[i].size(); ++j)
                {
                    if (area_a[i][j].data != 0 && area_a[i][j].status == 1)
                    {
                        if (j == 0 || (area_a[i][j - 1].data != 0 && area_a[i][j - 1].status == 0))
                        {
                            isCanLeft = false;
                            break;
                        }
                    }
                }
                if (!isCanLeft)
                    break;
            }
            if (isCanLeft)
            {
                for (int i = 0; i < area_a.size(); ++i)
                {
                    for (int j = 0; j < area_a[i].size(); ++j)
                    {
                        if (area_a[i][j].data != 0 && area_a[i][j].status == 1)
                        {
                            area_a[i][j - 1] = area_a[i][j];
                            area_a[i][j] = {0, 0};
                        }
                    }
                }
            }
            break;
        }
        case 2:
        {
            bool isCanRight = true;
            for (int i = 0; i < area_a.size(); ++i)
            {
                for (int j = 0; j < area_a[i].size(); ++j)
                {
                    if (area_a[i][j].data != 0 && area_a[i][j].status == 1)
                    {
                        if (j == area_a[i].size() - 1 || (area_a[i][j + 1].data != 0 && area_a[i][j + 1].status == 0))
                        {
                            isCanRight = false;
                            break;
                        }
                    }
                }
                if (!isCanRight)
                    break;
            }
            if (isCanRight)
            {
                for (int i = 0; i < area_a.size(); ++i)
                {
                    for (int j = area_a[i].size(); j >= 0; --j)
                    {
                        if (area_a[i][j].data != 0 && area_a[i][j].status == 1)
                        {
                            area_a[i][j + 1] = area_a[i][j];
                            area_a[i][j] = {0, 0};
                        }
                    }
                }
            }
            break;
        }
        case 3:
            progress();
            break;
        default:
            break;
        }
    }

    void spin()
    {
        int data = 1;
        std::vector<std::pair<int, int>> coords;
        std::vector<std::pair<int, int>> new_coords;
        int minx = INT_MAX;
        int miny = INT_MAX;
        int maxx = INT_MIN;
        int maxy = INT_MIN;
        for (int i = 0; i < area_a.size(); ++i)
        {
            for (int j = 0; j < area_a[i].size(); ++j)
            {
                if (area_a[i][j].status == 1 && area_a[i][j].data != 0)
                {
                    data = area_a[i][j].data;
                    minx = std::min(minx, j);
                    miny = std::min(miny, i);
                    maxx = std::max(maxx, j);
                    maxy = std::max(maxy, i);

                    coords.push_back(std::make_pair(j, i));
                }
            }
        }

        int centerx = (minx + maxx) >> 1;
        int centery = (miny + maxy) >> 1;

        for (auto& coord : coords)
        {
            std::pair<int, int> new_coord = std::make_pair(centerx + coord.second - centery, centery - (coord.first - centerx));

            if (new_coord.second >= area_a.size() ||
                new_coord.first >= area_a[0].size() ||
                new_coord.second < 0 ||
                new_coord.first < 0 ||
                (area_a[new_coord.second][new_coord.first].data != 0 &&
                area_a[new_coord.second][new_coord.first].status == 0))
            {
                return; // 越界
            }
            new_coords.push_back(new_coord);
        }

        for (auto& coord : coords)
        {
            area_a[coord.second][coord.first].status = 0;
            area_a[coord.second][coord.first].data = 0;
        }
        for (auto& coord : new_coords)
        {
            area_a[coord.second][coord.first].status = 1;
            area_a[coord.second][coord.first].data = data;
        }
    }

    // 结算
    void settle()
    {
        int fall = 0;
        for (int i = area_a.size(); i >= 0; --i)
        {
            int sum = 0;
            for (int j = 0; j < area_a[i].size(); ++j)
            {
                sum += area_a[i][j].data != 0 ? 1 : 0;
            }
            if (sum == area_a[i].size() && sum != 0)
            {
                ++score;
                ++fall;
            }
            else if (fall > 0)
            {
                for (int j = 0; j < area_a[i].size(); ++j)
                {
                    if (i + fall >= 0 || i + fall < area_a.size())
                    {
                        area_a[i + fall][j] = area_a[i][j];
                        area_a[i][j] = {0, 0};
                    }
                }
            }
            m_class = 1 + score / 10;
        }
    }

    void reset()
    {
        isGG = false;
        isInit = false;
        isFalling = false;
        score = 0;
        m_class = 1;
    }

    void gameOver()
    {
        isGG = true;

        for (int i = 0; i < area_a.size(); ++i)
        {
            for (int j = 0; j < area_a[i].size(); ++j)
            {
                area_a[i][j] = {0, 0};
            }
        }

        for (int i = 0; i < area_b.size(); ++i)
        {
            for (int j = 0; j < area_b[i].size(); ++j)
            {
                area_b[i][j] = 0;
            }
        }
    }

    void progress()
    {
        if (isGG) return;
        if (!isInit)
        {
            isInit = true;
            random4areaB();
        }

        if (isFalling)
        {
            {
                // 首先清理预测落点数据
                for (int i = 0; i < area_a.size(); ++i)
                {
                    for (int j = 0; j < area_a[i].size(); ++j)
                    {
                        if (area_a[i][j].status == 2)
                        {
                            area_a[i][j] = {0, 0};
                        }
                    }
                }
                // 下落状态预测落点
                // 1.收集活动块颜色和位置数据集
                int data = 1;
                std::vector<std::pair<int, int>> coords;
                for (int i = 0; i < area_a.size(); ++i)
                {
                    for (int j = 0; j < area_a[i].size(); ++j)
                    {
                        if (area_a[i][j].status == 1 && area_a[i][j].data != 0)
                        {
                            data = area_a[i][j].data;
                            coords.push_back(std::make_pair(j, i));
                        }
                    }
                }
                // 2. 将活动快落点整体增加，直到碰到非活动快或者底边为止
                int diffy;
                for (diffy = 0;; ++diffy)
                {
                    bool isBreak = false;
                    for (auto& coord : coords)
                    {
                        if (area_a[coord.second + diffy][coord.first].data != 0
                         && area_a[coord.second + diffy][coord.first].status == 0)
                        {
                            --diffy;
                            isBreak = true;
                            break;
                        }
                        if (coord.second + diffy == area_a.size() - 1)
                        {
                            isBreak = true;
                            break;
                        }
                    }
                    if (isBreak) break;
                }

                for (auto& coord : coords)
                {
                    if (area_a[coord.second + diffy][coord.first].data != 0)
                        continue;

                    area_a[coord.second + diffy][coord.first].status = 2;
                    area_a[coord.second + diffy][coord.first].data = data;
                }
            }

            for (int i = 0; i < area_a.size(); ++i)
            {
                for (int j = 0; j < area_a[i].size(); ++j)
                {
                    if (area_a[i][j].status == 1)
                    {
                        // 1. 下一级为底边
                        if (i + 1 == area_a.size())
                        {
                            isFalling = false;
                            break;
                        }
                        // 2. 下一级为固定块
                        else if (area_a[i + 1][j].data != 0 && area_a[i + 1][j].status == 0)
                        {
                            isFalling = false;
                            break;
                        }
                    }
                }
                if (!isFalling)
                {
                    break;
                }
            }

            // 如果当前非静止态
            if (isFalling)
            {
                // 将当前方块落到下一级
                for (int i = area_a.size(); i >= 0; --i)
                {
                    for (int j = 0; j < area_a[i].size(); ++j)
                    {
                        if (area_a[i][j].data != 0 && area_a[i][j].status == 1)
                        {
                            area_a[i + 1][j] = area_a[i][j];
                            area_a[i][j] = {0, 0};
                        }
                    }
                }
            }
            else
            {
                // 当前非下落状态则清空status
                for (int i = 0; i < area_a.size(); ++i)
                {
                    for (int j = 0; j < area_a[i].size(); ++j)
                    {
                        if (area_a[i][j].status == 1)
                            area_a[i][j].status = 0;
                    }
                }
            }
        }

        // 当活动块落到固定块或者地面
        if (!isFalling)
        {
            settle();

            isFalling = true;

            for (int i = 0; i < BLOCK_SIZE; ++i)
            {
                for (int j = 0; j < BLOCK_SIZE; ++j)
                {
                    if (area_b[i][j] != 0)
                    {
                        if (area_a[i][((10 - BLOCK_SIZE) >> 1) - 1 + j].data != 0 && area_a[i][((10 - BLOCK_SIZE) >> 1) - 1 + j].status == 0)
                        {
                            gameOver();
                            return;
                        }
                        area_a[i][((10 - BLOCK_SIZE) >> 1) - 1 + j] = {area_b[i][j], 1};
                    }
                }
            }

            // 首次由b点召唤方块到a点
            {
                // 首先清理预测落点数据
                for (int i = 0; i < area_a.size(); ++i)
                {
                    for (int j = 0; j < area_a[i].size(); ++j)
                    {
                        if (area_a[i][j].status == 2)
                        {
                            area_a[i][j] = {0, 0};
                        }
                    }
                }
                // 下落状态预测落点
                // 1.收集活动块颜色和位置数据集
                int data = 1;
                std::vector<std::pair<int, int>> coords;
                for (int i = 0; i < area_a.size(); ++i)
                {
                    for (int j = 0; j < area_a[i].size(); ++j)
                    {
                        if (area_a[i][j].status == 1 && area_a[i][j].data != 0)
                        {
                            data = area_a[i][j].data;
                            coords.push_back(std::make_pair(j, i));
                        }
                    }
                }
                // 2. 将活动快落点整体增加，直到碰到非活动快或者底边为止
                int diffy;
                for (diffy = 0;; ++diffy)
                {
                    bool isBreak = false;
                    for (auto& coord : coords)
                    {
                        if (area_a[coord.second + diffy][coord.first].data != 0
                         && area_a[coord.second + diffy][coord.first].status == 0)
                        {
                            --diffy;
                            isBreak = true;
                            break;
                        }
                        if (coord.second + diffy == area_a.size() - 1)
                        {
                            isBreak = true;
                            break;
                        }
                    }
                    if (isBreak) break;
                }

                for (auto& coord : coords)
                {
                    if (area_a[coord.second + diffy][coord.first].data != 0)
                        continue;

                    area_a[coord.second + diffy][coord.first].status = 2;
                    area_a[coord.second + diffy][coord.first].data = data;
                }
            }

            // 延迟用来生成其他随机数
            usleep(1000);
            random4areaB();
        }
    }

    void renderColorBlock(std::string& s, std::string& unit, int type)
    {
        s.append("\033[3");
        char type_ = '0' + type;
        s.append(1, type_);
        s.append("m");
        s.append(unit);
        s.append("\033[0m");
    }
    void renderColorBlock(std::string& s,
        std::string& unit,
        std::string& unit_prediction,
        int status,
        int type)
    {
        s.append("\033[3");
        char type_ = '0' + type;
        s.append(1, type_);
        s.append("m");
        if (status == 2)
        {
            s.append(unit_prediction);
        }
        else
        {
            s.append(unit);
        }
        s.append("\033[0m");
    }

    std::string render()
    {
        std::string s = "";
        std::string unit = "██";
        std::string unit_short = "▇▇";
        std::string unit_prediction = "▤▤";

        std::string blank = "  ";
        // =========================第一排框

        // a区上方
        s.append(unit);
        for (int i = 0; i < cfg.width; ++i)
        {
            s.append(unit);
        }
        s.append(unit);
        // b区上方
        s.append(unit);
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            s.append(unit);
        }
        s.append(unit);
        // 结束
        s.append(unit);
        s.append("\n");

        // ========================= b区域存在的框
        
        for (int j = 0; j < BLOCK_SIZE + 2; ++j)
        {
            // a区
            s.append(unit);
            for (int i = 0; i < cfg.width; ++i)
            {
                if (area_a[j][i].data != 0)
                    renderColorBlock(s, unit_short, unit_prediction, area_a[j][i].status, area_a[j][i].data);
                    // s.append(unit);
                else
                    s.append(blank);
            }
            s.append(unit);
            // b区
            s.append(blank);
            for (int i = 0; i < BLOCK_SIZE; ++i)
            {
                if (j > 0 && j < BLOCK_SIZE + 2 - 1)
                {
                    if (area_b[j - 1][i] != 0)
                    {
                        renderColorBlock(s, unit_short, area_b[j - 1][i]);
                    }
                    else
                        s.append(blank);
                }
                else
                {
                    s.append(blank);
                }
            }
            s.append(blank);
            // 结束
            s.append(unit);
            s.append("\n");
        }
        // =========================分割框

        // a区
        s.append(unit);
        for (int i = 0; i < cfg.width; ++i)
        {
            if (area_a[BLOCK_SIZE + 2][i].data != 0)
                renderColorBlock(s, unit_short, unit_prediction, area_a[BLOCK_SIZE + 2][i].status, area_a[BLOCK_SIZE + 2][i].data);
                // s.append(unit);
            else
                s.append(blank);
        }
        s.append(unit);
        // b区
        s.append(unit);
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            s.append(unit);
        }
        s.append(unit);
        // 结束
        s.append(unit);
        s.append("\n");

        // =========================剩余的a区

        for (int j = 0; j < cfg.height - BLOCK_SIZE - 2 - 1; ++j)
        {
            // a区
            s.append(unit);
            for (int i = 0; i < cfg.width; ++i)
            {
                if (area_a[BLOCK_SIZE + 2 + 1 + j][i].data != 0)
                    renderColorBlock(s, unit_short, unit_prediction, area_a[BLOCK_SIZE + 2 + 1 + j][i].status, area_a[BLOCK_SIZE + 2 + 1 + j][i].data);
                    // s.append(unit);
                else
                    s.append(blank);
            }
            s.append(unit);
            // c区
            s.append(blank);
            char score_buf[16];
            char m_class_buf[16];
            sprintf(score_buf, "%08d", score);
            sprintf(m_class_buf, "等级：%04d", m_class);
            
            for (int i = 0; i < BLOCK_SIZE; ++i)
            {
                if (j == 1 && i == 0)
                {
                    s.append("分数:");
                }
                else if (j == 2 && i == 1)
                {
                    s.append(score_buf);
                }
                else if (j == 3 && i == 0)
                {
                    s.append(m_class_buf);
                }
                else if (j == 5 && i == 1 && isGG)
                {
                    s.append("game over");
                }
                else if (j == 7 && i == 0 && isGG)
                {
                    s.append("请按下r键重开");
                }
                else
                {
                    s.append(blank);
                }
            }
            s.append(blank);
            // 结束
            s.append(blank);
            s.append("\n");
        }
        // =========================底线

        // a区底
        s.append(unit);
        for (int i = 0; i < cfg.width; ++i)
        {
            s.append(unit);
        }
        s.append(unit);
        // b区
        s.append(blank);
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            s.append(blank);
        }
        s.append(blank);
        // 结束
        s.append(blank);
        s.append("\n");

        return s;
    }

    std::string temp = 
    "██████████████████████████████████████\n"
    "██                    ██            ██\n"
    "██                    ██            ██\n"
    "██                    ██            ██\n"
    "██                    ██            ██\n"
    "██                    ██            ██\n"
    "██                    ██            ██\n"
    "██                    ████████████████\n"
    "██                    ██              \n"
    "██                    ██  分数:        \n"
    "██                    ██  00000000000 \n"
    "██                    ██  等级：000000 \n"
    "██                    ██              \n"
    "██                    ██   game over  \n"
    "██                    ██              \n"
    "██                    ██  请按下r键重开 \n"
    "██                    ██              \n"
    "██                    ██              \n"
    "██                    ██              \n"
    "██                    ██              \n"
    "██                    ██              \n"
    "████████████████████████              \n"
    ;

};

static gameManager gm;

void* op(void* args)
{
    init_keyboard();
    for(;;)
    {
        if (kbhit())
        {
            switch (readch())
            {
            case 119: // w
                gm.spin();
                break;
            case 97: // a
                gm.move(1);
                break;
            case 115: // s
                gm.move(3);
                break;
            case 100: // d
                gm.move(2);
                break;
            case 114: // r
                if (gm.isGG)
                {
                    gm.reset();
                }
                break;
            default:
                break;
            }
        }
    }
    close_keyboard();
}

void init()
{
    gm.cfg.width = 10;
    gm.cfg.height = 20;
    gm.cfg.items = new int[7] {0xf0, 0x170, 0x470, 0x270, 0x660, 0x462, 0x264};

    gm.init();

    // printf("%d, %d\n", gm.area_a.size(), gm.area_a[0].size());
    // printf("%d, %d\n", gm.area_b.size(), gm.area_b[0].size());

    static pthread_t th;

    int ret = pthread_create(&th, NULL, op, NULL);
    if (ret != 0)
    {
        printf("pthread_create error: error_code= %d\n", ret);
    }
}

void update()
{
    usleep(1000000 / gm.m_class);

    gm.progress();

}

void render()
{
    clearScreen();

    printf("%s", gm.render().c_str());
}

void bye()
{
}
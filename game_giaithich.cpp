// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                     SUDOKU LITE - HUST PROJECT                          ║
// ║  Sinh viên: Bùi Duy Vương - MSSV: 20223832                             ║
// ║  Môn học  : Đồ án thiết kế I                                   ║
// ║  Mô tả    : Game Sudoku với 2 chế độ Luyện Tập và Thử Thách (2 Loại)  ║
// ║  Thư viện : SFML (Simple and Fast Multimedia Library)                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝

#include <SFML/Graphics.hpp>  // Thư viện đồ họa SFML: cửa sổ, hình chữ nhật, văn bản
#include <iostream>           // Xuất thông báo lỗi ra console
#include <vector>             // Mảng động std::vector
#include <array>              // Mảng tĩnh std::array (dùng cho lưới 9x9)
#include <string>             // Xử lý chuỗi ký tự
#include <chrono>             // Lấy thời gian hệ thống để khởi tạo seed ngẫu nhiên
#include <algorithm>          // std::shuffle, std::sort, std::reverse
#include <random>             // Bộ sinh số ngẫu nhiên std::mt19937
#include <iomanip>            // std::setfill, std::setw cho định dạng thời gian
#include <sstream>            // std::ostringstream để tạo chuỗi định dạng

// ─── Hằng số kích thước giao diện ────────────────────────────────────────────
const int CELL_SIZE    = 60;                              // Kích thước mỗi ô Sudoku (pixel)
const int GRID_OFFSET  = 40;                              // Khoảng cách từ mép cửa sổ đến lưới
const int BOARD_SIZE   = CELL_SIZE * 9;                   // Tổng chiều dài lưới 9x9 = 540px
const int WINDOW_WIDTH  = BOARD_SIZE + GRID_OFFSET * 2;  // Chiều rộng cửa sổ = 620px
const int WINDOW_HEIGHT = BOARD_SIZE + GRID_OFFSET * 2 + 160; // Chiều cao cửa sổ = 780px (bao gồm thanh dashboard phía dưới)

// ─── Enum trạng thái màn hình ─────────────────────────────────────────────────
// Mỗi giá trị tương ứng với một màn hình khác nhau trong game
enum class GameMode { 
    MAIN_MENU,             // Màn hình Menu chính
    CHALLENGE_TYPE_SELECT, // Màn hình chọn Loại Thử Thách (Loại 1 hoặc Loại 2)
    CHALLENGE_SELECT,      // Màn hình chọn Cấp độ (1~8)
    PLAYING_PRACTICE,      // Đang chơi chế độ Luyện Tập
    PLAYING_CHALLENGE,     // Đang chơi chế độ Thử Thách
    GAME_OVER,             // Màn hình thua cuộc
    VICTORY,               // Màn hình chiến thắng
    HISTORY_MENU           // Màn hình Bảng Xếp Hạng / Lịch Sử
};

// ─── Struct lưu thông tin một ván chơi vào lịch sử ───────────────────────────
struct HistoryEntry {
    std::string modeName;  // Tên chế độ: "Luyen Tap" hoặc "Thu Thach"
    int duration;          // Thời gian đã chơi (tính bằng giây)
    int wrongMoves;        // Tổng số lần điền sai
    bool isWin;            // true = thắng, false = thua
    int level;             // Cấp độ (1~8); = 0 nếu là Luyện Tập
    int challengeType;     // 1 = Đếm lỗi, 2 = Giới hạn thời gian; = 0 nếu là Luyện Tập
};

// ════════════════════════════════════════════════════════════════════════════════
//  Class SudokuGenerator — Sinh đề Sudoku hợp lệ, đảm bảo đơn nghiệm
// ════════════════════════════════════════════════════════════════════════════════
class SudokuGenerator {
    std::mt19937 rng; // Bộ sinh số ngẫu nhiên Mersenne Twister (chất lượng cao)
    using Grid = std::array<std::array<int,9>,9>; // Bí danh kiểu cho lưới 9x9

    // Kiểm tra xem có thể đặt số n vào ô (r,c) hợp lệ không
    // (theo luật Sudoku: không trùng hàng, cột, ô 3x3)
    bool valid(const Grid& b, int r, int c, int n) const {
        // Kiểm tra hàng r và cột c
        for (int i=0; i<9; i++) { if(b[r][i]==n || b[i][c]==n) return false; }
        // Kiểm tra ô 3x3 chứa vị trí (r,c)
        int br=r/3*3, bc=c/3*3;
        for (int i=0; i<3; i++) for(int j=0; j<3; j++) if(b[br+i][bc+j]==n) return false;
        return true;
    }

    // Điền ngẫu nhiên toàn bộ lưới 9x9 bằng đệ quy (backtracking)
    // Trả về true nếu điền thành công
    bool fillRandom(Grid& b) {
        for (int r=0; r<9; r++) for(int c=0; c<9; c++) if(b[r][c]==0) {
            std::vector<int> ns={1,2,3,4,5,6,7,8,9};
            std::shuffle(ns.begin(), ns.end(), rng); // Xáo trộn để kết quả ngẫu nhiên mỗi lần chơi
            for (int n:ns) { if(valid(b,r,c,n)){ b[r][c]=n; if(fillRandom(b)) return true; b[r][c]=0; } }
            return false; // Không có số nào hợp lệ → quay lui
        }
        return true; // Toàn bộ lưới đã điền xong
    }

    // Đếm số nghiệm của bảng hiện tại (dừng lại khi cnt >= 2 để tiết kiệm thời gian)
    // Dùng để kiểm tra tính đơn nghiệm khi tạo đề
    void countSol(Grid b, int& cnt) {
        if (cnt >= 2) return; // Đã biết có nhiều hơn 1 nghiệm → không cần đếm thêm
        for (int r=0; r<9; r++) for(int c=0; c<9; c++) if(b[r][c]==0) {
            for (int n=1; n<=9; n++) if(valid(b,r,c,n)) { b[r][c]=n; countSol(b,cnt); if(cnt>=2) return; }
            return; // Ô trống này không điền được số nào hợp lệ → nhánh này bế tắc
        }
        cnt++; // Điền xong toàn bộ → tìm được 1 nghiệm
    }

public:
    // Khởi tạo với seed = thời gian hiện tại → mỗi lần chạy game cho đề khác nhau
    SudokuGenerator() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    // Sinh một lưới Sudoku đầy đủ hợp lệ (81 ô đã điền)
    Grid generateFull() {
        Grid b{}; for(auto& r:b) r.fill(0); // Khởi tạo lưới rỗng
        fillRandom(b); return b;
    }

    // Từ lưới đầy đủ, xóa tối đa số ô nhất có thể sao cho vẫn đảm bảo đơn nghiệm
    // Kết quả: đề Sudoku khó nhất có thể (thường còn ~17-25 ô)
    Grid generateMaxPuzzle(const Grid& full) {
        Grid p = full;
        std::vector<std::pair<int,int>> cells;
        for(int r=0; r<9; r++) for(int c=0; c<9; c++) cells.push_back({r,c});
        std::shuffle(cells.begin(), cells.end(), rng); // Xáo trộn thứ tự xóa để kết quả đa dạng

        for (auto& [r,c]: cells) {
            int bak = p[r][c]; p[r][c] = 0; // Thử xóa ô này
            int cnt = 0; countSol(p, cnt);
            if (cnt != 1) p[r][c] = bak;    // Nếu mất đơn nghiệm → khôi phục ô đó
            // Nếu vẫn đơn nghiệm → giữ nguyên (ô đã bị xóa)
        }
        return p;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
//  Class SudokuBoard — Quản lý toàn bộ trạng thái bảng đang chơi
// ════════════════════════════════════════════════════════════════════════════════
class SudokuBoard {
public:
    using Grid = std::array<std::array<int,9>,9>;
    Grid puzzle;   // Đề bài gốc (ô = 0 nghĩa là ô trống, người chơi cần điền)
    Grid solution; // Đáp án đúng hoàn chỉnh (dùng để kiểm tra và hiển thị khi thua)
    Grid player;   // Các số người chơi đã điền vào
    std::array<std::array<bool,9>,9> err; // err[r][c] = true nếu người chơi điền sai ô (r,c)

    SudokuBoard() { reset(); } // Constructor: khởi tạo bảng rỗng

    // Đặt lại toàn bộ bảng về trạng thái ban đầu (tất cả = 0 / false)
    void reset() {
        for(auto& r:puzzle)   r.fill(0);
        for(auto& r:solution) r.fill(0);
        for(auto& r:player)   r.fill(0);
        for(auto& r:err)      r.fill(false);
    }

    // Kiểm tra ô (r,c) có phải ô đề bài cho sẵn không (không được phép sửa)
    bool isFixed(int r, int c) const { return puzzle[r][c] != 0; }

    // Lấy giá trị hiển thị tại ô (r,c): ưu tiên ô đề bài, sau đó là ô người chơi đã điền
    int  get(int r, int c)     const { return isFixed(r,c) ? puzzle[r][c] : player[r][c]; }

    // Người chơi điền giá trị v vào ô (r,c)
    // Trả về true nếu điền đúng (hoặc xóa), false nếu điền sai
    bool set(int r, int c, int v) {
        if(isFixed(r,c)) return true;      // Ô đề bài → không cho sửa, báo "hợp lệ" để không tính lỗi
        player[r][c] = v;
        if(v == 0){ err[r][c] = false; return true; } // Xóa ô → xóa trạng thái lỗi
        err[r][c] = (v != solution[r][c]); // Đánh dấu lỗi nếu điền sai đáp án
        return !err[r][c];
    }

    // Kiểm tra người chơi đã điền đúng và đầy đủ toàn bộ bảng chưa
    bool complete() const {
        for(int r=0; r<9; r++) for(int c=0; c<9; c++) if(get(r,c) != solution[r][c]) return false;
        return true;
    }
};

// Hàm tiện ích: chuyển số giây thành chuỗi định dạng "MM:SS"
std::string formatTime(int totalSeconds) {
    if (totalSeconds < 0) return "00:00"; // Tránh hiển thị số âm
    int mins = totalSeconds / 60;
    int secs = totalSeconds % 60;
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << mins << ":" << std::setw(2) << secs;
    return ss.str();
}

// ════════════════════════════════════════════════════════════════════════════════
//  Hàm main — Điểm khởi đầu chương trình
// ════════════════════════════════════════════════════════════════════════════════
int main() {
    // Tạo cửa sổ SFML với kích thước đã định, giới hạn 60 FPS để tiết kiệm CPU
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Sudoku Lite - HUST Project");
    window.setFramerateLimit(60);

    // Nạp font chữ Arial từ file; nếu thiếu file → báo lỗi và thoát
    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cout << "Loi font chu arial.ttf!\n";
        return -1;
    }

    // Khai báo các đối tượng chính
    SudokuGenerator generator; // Bộ sinh đề Sudoku
    SudokuBoard board;          // Bảng đang chơi
    GameMode mode = GameMode::MAIN_MENU; // Khởi đầu ở màn hình Menu chính
    std::vector<HistoryEntry> history;   // Danh sách lịch sử các ván đã chơi (lưu trong RAM, reset khi thoát game)

    // Biến trạng thái ván chơi
    int selectedRow = -1, selectedCol = -1; // Ô đang được chọn (-1 = chưa chọn ô nào)
    int wrongMoves = 0;    // Số lần điền sai trong ván hiện tại
    int challengeLevel = 1; // Cấp độ thử thách đang chơi (1~8)
    int challengeType  = 1; // Loại thử thách: 1 = Đếm lỗi | 2 = Chỉ giới hạn thời gian

    // ─── Cấu hình Loại 1 (Đếm lỗi): thời gian + số lượt sai tối đa theo từng cấp độ ───
    // Cấp độ:              1     2     3     4     5    6    7    8
    int maxTimeByLvl_T1[]  = {2400, 2100, 1800, 1500, 1200, 900, 720, 540}; // Giây (ví dụ: cấp 1 = 40 phút)
    int maxWrongByLvl_T1[] = {  24,   18,   14,   10,    5,   3,   1,   0}; // Số lượt sai cho phép

    // ─── Cấu hình Loại 2 (Chỉ giới hạn thời gian): siết giờ hơn vì không có phản hồi đúng/sai ───
    // Cấp độ:              1     2     3    4    5    6    7    8
    int maxTimeByLvl_T2[]  = {1800, 1500, 1080, 900, 720, 540, 360, 270}; // Giây

    // Biến thời gian
    sf::Clock gameClock;       // Đồng hồ SFML bắt đầu đếm khi ván chơi khởi động
    int elapsedSeconds   = 0;  // Số giây đã trôi qua kể từ khi bắt đầu ván
    int countdownSeconds = 0;  // Số giây còn lại (dùng cho chế độ Thử Thách)

    // Cờ trạng thái đặc biệt
    bool isGameOverReasonTime = false; // true = thua do hết giờ, false = thua do sai quá nhiều
    bool revealAnswer = false;         // true = người chơi đã bấm "Xem đáp án" ở màn GAME_OVER

    // ─── Khởi tạo các nút UI cho Menu chính ─────────────────────────────────
    sf::RectangleShape btnPractice(sf::Vector2f(260, 50)), btnChallengeMenu(sf::Vector2f(260, 50)), btnHistory(sf::Vector2f(260, 50));
    btnPractice.setPosition(WINDOW_WIDTH/2 - 130, 240);       btnPractice.setFillColor(sf::Color(52, 152, 219));    // Xanh dương
    btnChallengeMenu.setPosition(WINDOW_WIDTH/2 - 130, 310);  btnChallengeMenu.setFillColor(sf::Color(230, 126, 34)); // Cam
    btnHistory.setPosition(WINDOW_WIDTH/2 - 130, 380);        btnHistory.setFillColor(sf::Color(149, 165, 166));    // Xám

    // ─── Nút chọn Loại thử thách (màn CHALLENGE_TYPE_SELECT) ────────────────
    // Chiều rộng 460px để chứa đủ cả tiêu đề và mô tả mà không bị cắt xén
    sf::RectangleShape btnType1(sf::Vector2f(460, 90)), btnType2(sf::Vector2f(460, 90));
    btnType1.setPosition(WINDOW_WIDTH/2 - 230, 220); btnType1.setFillColor(sf::Color(41, 128, 185));  // Xanh đậm → Loại 1
    btnType2.setPosition(WINDOW_WIDTH/2 - 230, 330); btnType2.setFillColor(sf::Color(192, 57, 43));   // Đỏ → Loại 2

    // ─── 8 nút chọn cấp độ (màn CHALLENGE_SELECT), xếp thành lưới 2 cột x 4 hàng ───
    std::vector<sf::RectangleShape> btnLevels(8);
    for (int i = 0; i < 8; i++) {
        btnLevels[i].setSize(sf::Vector2f(130, 50));
        btnLevels[i].setFillColor(sf::Color(46, 204, 113)); // Xanh lá
        int row = i / 2; // Hàng: 0~3
        int col = i % 2; // Cột: 0 (trái) hoặc 1 (phải)
        float x = (col == 0) ? (WINDOW_WIDTH / 2 - 145) : (WINDOW_WIDTH / 2 + 15);
        float y = 200 + row * 70;
        btnLevels[i].setPosition(x, y);
    }

    // ─── Nút "Quay lại" dùng chung cho các màn menu phụ ─────────────────────
    sf::RectangleShape btnBackCommon(sf::Vector2f(150, 40));
    btnBackCommon.setPosition(WINDOW_WIDTH / 2 - 75, 530); // Căn giữa, gần cuối màn hình
    btnBackCommon.setFillColor(sf::Color(127, 140, 141));   // Xám

    // ─── Nút "<< MENU" hiển thị trong lúc đang chơi (góc phải dashboard) ────
    sf::RectangleShape btnBackInGame(sf::Vector2f(150, 35));
    btnBackInGame.setPosition(WINDOW_WIDTH - 170, BOARD_SIZE + GRID_OFFSET + 20);
    btnBackInGame.setFillColor(sf::Color(127, 140, 141));

    // ─── Nút trên màn GAME_OVER (chưa xem đáp án): 2 lựa chọn song song ────
    sf::RectangleShape btnViewAnswer(sf::Vector2f(180, 45));  // Nút "Xem đáp án" (cam)
    btnViewAnswer.setPosition(WINDOW_WIDTH/2 - 190, 330); btnViewAnswer.setFillColor(sf::Color(230, 126, 34));
    sf::RectangleShape btnBackFromGameOver(sf::Vector2f(180, 45)); // Nút "Về Menu" (xám)
    btnBackFromGameOver.setPosition(WINDOW_WIDTH/2 + 10, 330); btnBackFromGameOver.setFillColor(sf::Color(127, 140, 141));

    // ─── Nút "Về Menu" cố định ở dưới cùng, xuất hiện SAU KHI đã xem đáp án ─
    sf::RectangleShape btnBackAfterReveal(sf::Vector2f(220, 40));
    btnBackAfterReveal.setPosition(WINDOW_WIDTH/2 - 110, WINDOW_HEIGHT - 50); // Gắn sát đáy cửa sổ
    btnBackAfterReveal.setFillColor(sf::Color(127, 140, 141));

    // ════════════════════════════════════════════════════════════════════════════
    //  VÒNG LẶP CHÍNH (GAME LOOP) — Chạy liên tục cho đến khi đóng cửa sổ
    // ════════════════════════════════════════════════════════════════════════════
    while (window.isOpen()) {

        // ── Phần 1: Xử lý sự kiện đầu vào ──────────────────────────────────
        sf::Event event;
        while (window.pollEvent(event)) {
            // Người dùng bấm nút X đóng cửa sổ
            if (event.type == sf::Event::Closed) window.close();

            // Lấy vị trí chuột trong tọa độ cửa sổ (pixel) và tọa độ thế giới
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            sf::Vector2f mouseCoords = window.mapPixelToCoords(mousePos);

            // ── 1. MAIN MENU: xử lý click vào 3 nút chính ──────────────────
            if (mode == GameMode::MAIN_MENU) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (btnPractice.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        // Sinh đề mới và bắt đầu chế độ Luyện Tập
                        auto full = generator.generateFull();
                        auto puzzle = generator.generateMaxPuzzle(full);
                        board.reset(); board.puzzle = puzzle; board.solution = full;
                        selectedRow = selectedCol = -1; wrongMoves = 0;
                        gameClock.restart(); mode = GameMode::PLAYING_PRACTICE;
                    }
                    else if (btnChallengeMenu.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        // Chuyển sang màn chọn Loại thử thách trước khi chọn cấp độ
                        mode = GameMode::CHALLENGE_TYPE_SELECT;
                    }
                    else if (btnHistory.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        mode = GameMode::HISTORY_MENU; // Mở bảng xếp hạng
                    }
                }
            }
            // ── 2. CHALLENGE_TYPE_SELECT: chọn Loại 1 hoặc Loại 2 ──────────
            else if (mode == GameMode::CHALLENGE_TYPE_SELECT) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (btnBackCommon.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        mode = GameMode::MAIN_MENU; // Quay lại Menu chính
                    } else if (btnType1.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        challengeType = 1;            // Chọn Loại 1 (đếm lỗi)
                        mode = GameMode::CHALLENGE_SELECT;
                    } else if (btnType2.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        challengeType = 2;            // Chọn Loại 2 (giới hạn thời gian)
                        mode = GameMode::CHALLENGE_SELECT;
                    }
                }
                // Phím Escape cũng quay lại Menu chính
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                    mode = GameMode::MAIN_MENU;
                }
            }
            // ── 3. CHALLENGE_SELECT: chọn 1 trong 8 cấp độ ─────────────────
            else if (mode == GameMode::CHALLENGE_SELECT) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (btnBackCommon.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        mode = GameMode::CHALLENGE_TYPE_SELECT; // Lùi về chọn lại Loại
                    }
                    // Kiểm tra click vào 8 nút cấp độ
                    for (int i = 0; i < 8; i++) {
                        if (btnLevels[i].getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                            challengeLevel = i + 1; // Gán cấp độ 1~8
                            // Sinh đề mới và bắt đầu ván thử thách
                            auto full = generator.generateFull();
                            auto puzzle = generator.generateMaxPuzzle(full);
                            board.reset(); board.puzzle = puzzle; board.solution = full;
                            selectedRow = selectedCol = -1; wrongMoves = 0;
                            isGameOverReasonTime = false;
                            revealAnswer = false;
                            gameClock.restart(); 
                            mode = GameMode::PLAYING_CHALLENGE;
                            break; // Chỉ xử lý 1 nút duy nhất mỗi click
                        }
                    }
                }
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                    mode = GameMode::CHALLENGE_TYPE_SELECT;
                }
            }
            // ── 4. HISTORY_MENU: bấm nút Quay Lại để về Menu chính ─────────
            else if (mode == GameMode::HISTORY_MENU) {
                if ((event.type == sf::Event::MouseButtonPressed && btnBackCommon.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) || 
                    (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)) {
                    mode = GameMode::MAIN_MENU;
                }
            }
            // ── 5. PLAYING (Luyện Tập & Thử Thách): xử lý click bảng và nhập phím ──
            else if (mode == GameMode::PLAYING_PRACTICE || mode == GameMode::PLAYING_CHALLENGE) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (btnBackInGame.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        mode = GameMode::MAIN_MENU; // Bấm nút "<< MENU" để thoát giữa chừng
                    }
                    // Click vào vùng lưới → cập nhật ô đang chọn
                    else if (mouseCoords.x >= GRID_OFFSET && mouseCoords.x < GRID_OFFSET + BOARD_SIZE &&
                             mouseCoords.y >= GRID_OFFSET && mouseCoords.y < GRID_OFFSET + BOARD_SIZE) {
                        selectedCol = (mouseCoords.x - GRID_OFFSET) / CELL_SIZE;
                        selectedRow = (mouseCoords.y - GRID_OFFSET) / CELL_SIZE;
                    }
                }

                // Xử lý nhập ký tự từ bàn phím
                if (event.type == sf::Event::TextEntered && selectedRow != -1 && selectedCol != -1) {
                    char enteredChar = static_cast<char>(event.text.unicode);
                    if (enteredChar >= '1' && enteredChar <= '9') {
                        // Chỉ điền vào ô không phải đề bài
                        if (!board.isFixed(selectedRow, selectedCol)) {
                            bool ok = board.set(selectedRow, selectedCol, enteredChar - '0');
                            if (!ok) {
                                // Điền sai: xử lý tùy theo chế độ
                                if (mode == GameMode::PLAYING_CHALLENGE && challengeType == 1) {
                                    // Loại 1: đếm lỗi; nếu vượt giới hạn → thua ngay
                                    wrongMoves++;
                                    if (wrongMoves > maxWrongByLvl_T1[challengeLevel - 1]) {
                                        history.push_back({"Thu Thach", elapsedSeconds, wrongMoves, false, challengeLevel, challengeType});
                                        revealAnswer = false;
                                        mode = GameMode::GAME_OVER;
                                    }
                                } else if (mode == GameMode::PLAYING_PRACTICE) {
                                    wrongMoves++; // Luyện tập: chỉ đếm lỗi để thống kê, không thua
                                }
                                // Loại 2: bỏ qua hoàn toàn, không đếm lỗi, không thua vì sai
                            }
                        }
                    } else if (enteredChar == '0' || enteredChar == 8) {
                        // Phím 0 hoặc Backspace: xóa số tại ô đang chọn
                        board.set(selectedRow, selectedCol, 0);
                    }
                }

                // Kiểm tra điều kiện chiến thắng sau mỗi lần điền
                if (board.complete()) {
                    if (mode == GameMode::PLAYING_PRACTICE) {
                        history.push_back({"Luyen Tap", elapsedSeconds, wrongMoves, true, 0, 0});
                    } else {
                        history.push_back({"Thu Thach", elapsedSeconds, wrongMoves, true, challengeLevel, challengeType});
                    }
                    mode = GameMode::VICTORY;
                }
            }
            // ── 6. GAME_OVER: cho phép xem đáp án hoặc về Menu trực tiếp ───
            else if (mode == GameMode::GAME_OVER) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (!revealAnswer) {
                        // Chưa xem đáp án → hiện 2 nút lựa chọn
                        if (btnViewAnswer.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                            revealAnswer = true; // Bật cờ hiển thị đáp án (KHÔNG rời màn hình)
                        } else if (btnBackFromGameOver.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                            mode = GameMode::MAIN_MENU; // Về Menu ngay không xem đáp án
                        }
                    } else {
                        // Đang xem đáp án → chỉ có nút "Về Menu" ở dưới cùng
                        if (btnBackAfterReveal.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                            revealAnswer = false;
                            mode = GameMode::MAIN_MENU;
                        }
                    }
                }
                // Escape cũng cho về Menu từ màn GAME_OVER
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                    revealAnswer = false;
                    mode = GameMode::MAIN_MENU;
                }
            }
            // ── 7. VICTORY: bấm bất kỳ để về Menu chính ────────────────────
            else if (mode == GameMode::VICTORY) {
                if (event.type == sf::Event::MouseButtonPressed || event.type == sf::Event::KeyPressed) {
                    mode = GameMode::MAIN_MENU;
                }
            }
        } // Kết thúc vòng xử lý sự kiện

        // ── Phần 2: Cập nhật trạng thái thời gian mỗi frame ────────────────
        if (mode == GameMode::PLAYING_PRACTICE) {
            // Luyện tập: đếm lên (không có giới hạn)
            elapsedSeconds = gameClock.getElapsedTime().asSeconds();
        } else if (mode == GameMode::PLAYING_CHALLENGE) {
            elapsedSeconds = gameClock.getElapsedTime().asSeconds();
            // Tính thời gian còn lại dựa vào Loại đang chơi
            int maxTime = (challengeType == 1) ? maxTimeByLvl_T1[challengeLevel - 1] : maxTimeByLvl_T2[challengeLevel - 1];
            countdownSeconds = maxTime - elapsedSeconds;
            if (countdownSeconds <= 0) {
                // Hết giờ → thua cuộc, lưu lịch sử và chuyển sang GAME_OVER
                isGameOverReasonTime = true;
                history.push_back({"Thu Thach", elapsedSeconds, wrongMoves, false, challengeLevel, challengeType});
                revealAnswer = false;
                mode = GameMode::GAME_OVER;
            }
        }

        // ════════════════════════════════════════════════════════════════════
        //  Phần 3: Vẽ giao diện (Rendering) — xóa màn hình rồi vẽ từng thành phần
        // ════════════════════════════════════════════════════════════════════
        window.clear(sf::Color(245, 242, 235)); // Màu nền kem nhạt (dễ nhìn, không chói)

        // ── SCREEN: MAIN MENU ────────────────────────────────────────────────
        if (mode == GameMode::MAIN_MENU) {
            // Tiêu đề game
            sf::Text title("SUDOKU LITE - HUST", font, 36);
            title.setStyle(sf::Text::Bold); title.setFillColor(sf::Color(44, 62, 80));
            title.setPosition(WINDOW_WIDTH/2 - title.getGlobalBounds().width/2, 120); // Căn giữa theo chiều ngang
            window.draw(title);

            // Vẽ 3 nút chính
            window.draw(btnPractice); window.draw(btnChallengeMenu); window.draw(btnHistory);

            // Lambda tiện ích: vẽ text căn giữa trên một nút hình chữ nhật
            auto drawMenuTxt = [&](const std::string& s, sf::RectangleShape& b) {
                sf::Text t(s, font, 18); t.setFillColor(sf::Color::White);
                t.setPosition(b.getPosition().x + b.getSize().x/2 - t.getGlobalBounds().width/2, b.getPosition().y + 14);
                window.draw(t);
            };
            drawMenuTxt("CHEDO LUYEN TAP", btnPractice);
            drawMenuTxt("CHEDO THU THACH", btnChallengeMenu);
            drawMenuTxt("BANG XEP HANG", btnHistory);
        }
        // ── SCREEN: CHỌN LOẠI THỬ THÁCH ─────────────────────────────────────
        else if (mode == GameMode::CHALLENGE_TYPE_SELECT) {
            sf::Text title("CHON LOAI THU THACH", font, 30);
            title.setStyle(sf::Text::Bold); title.setFillColor(sf::Color(230, 126, 34));
            title.setPosition(WINDOW_WIDTH/2 - title.getGlobalBounds().width/2, 130);
            window.draw(title);

            // Nút Loại 1 (xanh) + tiêu đề và mô tả
            window.draw(btnType1);
            sf::Text t1("LOAI 1: DEM LOI SAI", font, 18); t1.setStyle(sf::Text::Bold); t1.setFillColor(sf::Color::White);
            t1.setPosition(btnType1.getPosition().x + 20, btnType1.getPosition().y + 12);
            sf::Text d1("Bao do dung/sai tung o. Sai qua so\nluot quy dinh theo cap do la THUA.", font, 13); d1.setFillColor(sf::Color(220, 230, 240));
            d1.setPosition(btnType1.getPosition().x + 20, btnType1.getPosition().y + 42);
            window.draw(t1); window.draw(d1);

            // Nút Loại 2 (đỏ) + tiêu đề và mô tả
            window.draw(btnType2);
            sf::Text t2("LOAI 2: GIOI HAN THOI GIAN", font, 18); t2.setStyle(sf::Text::Bold); t2.setFillColor(sf::Color::White);
            t2.setPosition(btnType2.getPosition().x + 20, btnType2.getPosition().y + 12);
            sf::Text d2("Khong bao dung/sai. Het gio ma chua\nxong bang la THUA, du dien sai bao nhieu.", font, 13); d2.setFillColor(sf::Color(250, 220, 215));
            d2.setPosition(btnType2.getPosition().x + 20, btnType2.getPosition().y + 42);
            window.draw(t2); window.draw(d2);

            // Nút quay lại
            window.draw(btnBackCommon);
            sf::Text tBack("QUAY LAI", font, 16); tBack.setFillColor(sf::Color::White);
            tBack.setPosition(btnBackCommon.getPosition().x + btnBackCommon.getSize().x/2 - tBack.getGlobalBounds().width/2, btnBackCommon.getPosition().y + 10);
            window.draw(tBack);
        }
        // ── SCREEN: CHỌN CẤP ĐỘ THỬ THÁCH ──────────────────────────────────
        else if (mode == GameMode::CHALLENGE_SELECT) {
            // Hiển thị Loại đã chọn trong tiêu đề để người chơi không nhầm
            std::string typeLabel = (challengeType == 1) ? "LOAI 1: DEM LOI SAI" : "LOAI 2: GIOI HAN THOI GIAN";
            sf::Text title("CHON CAP DO - " + typeLabel, font, 22);
            title.setStyle(sf::Text::Bold); title.setFillColor(sf::Color(230, 126, 34));
            title.setPosition(WINDOW_WIDTH/2 - title.getGlobalBounds().width/2, 110);
            window.draw(title);

            // Vẽ 8 nút cấp độ, mỗi nút có tên và thông tin giới hạn (thời gian / lượt sai)
            for (int i = 0; i < 8; i++) {
                window.draw(btnLevels[i]);
                // Dòng phụ: hiển thị giới hạn tương ứng với Loại đang chọn
                std::string sub = (challengeType == 1)
                    ? (formatTime(maxTimeByLvl_T1[i]) + " | Loi<=" + std::to_string(maxWrongByLvl_T1[i]))
                    : (formatTime(maxTimeByLvl_T2[i]) + " | Khong bao loi");
                sf::Text t("LV " + std::to_string(i + 1), font, 16);
                t.setFillColor(sf::Color::White); t.setStyle(sf::Text::Bold);
                t.setPosition(btnLevels[i].getPosition().x + 10, btnLevels[i].getPosition().y + 6);
                sf::Text tSub(sub, font, 11); tSub.setFillColor(sf::Color(235, 245, 235));
                tSub.setPosition(btnLevels[i].getPosition().x + 10, btnLevels[i].getPosition().y + 28);
                window.draw(t); window.draw(tSub);
            }

            // Nút quay lại (về chọn Loại)
            window.draw(btnBackCommon);
            sf::Text tBack("QUAY LAI", font, 16); tBack.setFillColor(sf::Color::White);
            tBack.setPosition(btnBackCommon.getPosition().x + btnBackCommon.getSize().x/2 - tBack.getGlobalBounds().width/2, btnBackCommon.getPosition().y + 10);
            window.draw(tBack);
        }
        // ── SCREEN: BẢNG XẾP HẠNG ───────────────────────────────────────────
        else if (mode == GameMode::HISTORY_MENU) {
            sf::Text title("BANG XEP HANG CHI TIET", font, 28);
            title.setFillColor(sf::Color(44, 62, 80)); title.setPosition(WINDOW_WIDTH/2 - title.getGlobalBounds().width/2, 40);
            window.draw(title);

            // Tách lịch sử thành 2 danh sách: Luyện Tập (thắng) và Thử Thách (tất cả)
            std::vector<HistoryEntry> practiceRank;
            std::vector<HistoryEntry> challengeHist;
            for (auto& h : history) {
                if (h.modeName == "Luyen Tap" && h.isWin) practiceRank.push_back(h);
                else if (h.modeName == "Thu Thach") challengeHist.push_back(h);
            }

            // Sắp xếp Luyện Tập: ít lỗi nhất → nếu bằng → thời gian ngắn nhất
            std::sort(practiceRank.begin(), practiceRank.end(), [](const HistoryEntry& a, const HistoryEntry& b){
                if (a.wrongMoves != b.wrongMoves) return a.wrongMoves < b.wrongMoves;
                return a.duration < b.duration;
            });

            // Hiển thị Top 5 Luyện Tập
            sf::Text tHead1("TOP 5 TRAN LUYEN TAP (IT LOI NHAT)", font, 16); tHead1.setFillColor(sf::Color(52, 152, 219)); tHead1.setPosition(40, 110); window.draw(tHead1);
            for(size_t i=0; i<5 && i<practiceRank.size(); i++) {
                std::string row = "Top " + std::to_string(i+1) + " -> Loi: " + std::to_string(practiceRank[i].wrongMoves) + " | Thoi gian: " + formatTime(practiceRank[i].duration);
                sf::Text tRow(row, font, 14); tRow.setFillColor(sf::Color::Black); tRow.setPosition(40, 140 + i*25); window.draw(tRow);
            }

            // Đảo ngược để hiển thị ván gần nhất lên đầu (mới nhất → cũ nhất)
            sf::Text tHead2("CAC TRAN THU THACH GAN NHAT", font, 16); tHead2.setFillColor(sf::Color(230, 126, 34)); tHead2.setPosition(40, 320); window.draw(tHead2);
            std::reverse(challengeHist.begin(), challengeHist.end());
            for(size_t i=0; i<5 && i<challengeHist.size(); i++) {
                std::string status = challengeHist[i].isWin ? "[THANG]" : "[THUA]";
                std::string typeTag = (challengeHist[i].challengeType == 1) ? "L1" : "L2"; // Loại 1 hay Loại 2
                std::string row = "LV " + std::to_string(challengeHist[i].level) + " (" + typeTag + ") | " + status + " | Tg: " + formatTime(challengeHist[i].duration) + " | Loi: " + std::to_string(challengeHist[i].wrongMoves);
                sf::Text tRow(row, font, 14); 
                tRow.setFillColor(challengeHist[i].isWin ? sf::Color(46, 204, 113) : sf::Color(231, 76, 60)); // Xanh lá = thắng, đỏ = thua
                tRow.setPosition(40, 350 + i*25); window.draw(tRow);
            }

            window.draw(btnBackCommon);
            sf::Text tBack("QUAY LAI", font, 16); tBack.setFillColor(sf::Color::White);
            tBack.setPosition(btnBackCommon.getPosition().x + btnBackCommon.getSize().x/2 - tBack.getGlobalBounds().width/2, btnBackCommon.getPosition().y + 10);
            window.draw(tBack);
        }
        // ── SCREEN: ĐANG CHƠI (Luyện Tập & Thử Thách) ──────────────────────
        else if (mode == GameMode::PLAYING_PRACTICE || mode == GameMode::PLAYING_CHALLENGE) {
            
            // Vẽ ô highlight (nền xanh trong suốt) tại ô người chơi đang chọn
            if (selectedRow != -1 && selectedCol != -1) {
                sf::RectangleShape hl(sf::Vector2f(CELL_SIZE, CELL_SIZE));
                hl.setPosition(GRID_OFFSET + selectedCol * CELL_SIZE, GRID_OFFSET + selectedRow * CELL_SIZE);
                hl.setFillColor(sf::Color(52, 152, 219, 60)); // Alpha = 60 (trong suốt)
                window.draw(hl);
            }

            // Cờ ẩn phản hồi màu sắc đúng/sai (dùng cho Loại 2)
            bool hideErrorFeedback = (mode == GameMode::PLAYING_CHALLENGE && challengeType == 2);

            // Vẽ tất cả các số trên bảng
            for (int r = 0; r < 9; r++) {
                for (int c = 0; c < 9; c++) {
                    int val = board.get(r, c);
                    if (val != 0) { // Chỉ vẽ ô có số (bỏ qua ô trống)
                        sf::Text text(std::to_string(val), font, 30);
                        if (board.isFixed(r, c)) {
                            text.setFillColor(sf::Color(44, 62, 80));      // Số đề bài: màu xanh đen đậm
                        } else if (!hideErrorFeedback && board.err[r][c]) {
                            text.setFillColor(sf::Color(231, 76, 60));     // Số sai (Loại 1 / Luyện tập): màu đỏ
                        } else {
                            text.setFillColor(sf::Color(41, 128, 185));    // Số người chơi điền đúng (hoặc Loại 2): màu xanh dương
                        }
                        // Căn giữa text trong ô
                        sf::FloatRect textRect = text.getLocalBounds();
                        text.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                        text.setPosition(GRID_OFFSET + c * CELL_SIZE + CELL_SIZE / 2.0f, GRID_OFFSET + r * CELL_SIZE + CELL_SIZE / 2.0f);
                        window.draw(text);
                    }
                }
            }

            // Vẽ lưới đường kẻ (đậm cho viền 3x3, mảnh cho ô 1x1)
            for (int i = 0; i <= 9; i++) {
                sf::RectangleShape vLine, hLine;
                if (i % 3 == 0) {
                    // Đường viền ô 3x3: dày 4px, màu xanh đen đậm
                    vLine.setSize(sf::Vector2f(4, BOARD_SIZE)); vLine.setFillColor(sf::Color(44, 62, 80));
                    hLine.setSize(sf::Vector2f(BOARD_SIZE, 4)); hLine.setFillColor(sf::Color(44, 62, 80));
                    vLine.setPosition(GRID_OFFSET + i * CELL_SIZE - 2, GRID_OFFSET);
                    hLine.setPosition(GRID_OFFSET, GRID_OFFSET + i * CELL_SIZE - 2);
                } else {
                    // Đường kẻ ô 1x1: dày 1px, màu xám nhạt
                    vLine.setSize(sf::Vector2f(1, BOARD_SIZE)); vLine.setFillColor(sf::Color(189, 195, 199));
                    hLine.setSize(sf::Vector2f(BOARD_SIZE, 1)); hLine.setFillColor(sf::Color(189, 195, 199));
                    vLine.setPosition(GRID_OFFSET + i * CELL_SIZE, GRID_OFFSET);
                    hLine.setPosition(GRID_OFFSET, GRID_OFFSET + i * CELL_SIZE);
                }
                window.draw(vLine); window.draw(hLine);
            }

            // Vẽ thanh thông số phía dưới bảng
            std::string statsStr;
            if (mode == GameMode::PLAYING_PRACTICE) {
                statsStr = "LUYEN TAP | T.gian: " + formatTime(elapsedSeconds) + " | So loi: " + std::to_string(wrongMoves);
            } else if (challengeType == 1) {
                // Thử Thách Loại 1: hiển thị thời gian đếm ngược và lỗi hiện tại / tối đa
                int maxW = maxWrongByLvl_T1[challengeLevel - 1];
                statsStr = "THU THACH LV " + std::to_string(challengeLevel) + " (L1) | Con lai: " + formatTime(countdownSeconds) + " | Loi: " + std::to_string(wrongMoves) + "/" + std::to_string(maxW);
            } else {
                // Thử Thách Loại 2: chỉ hiển thị thời gian, không hiển thị lỗi
                statsStr = "THU THACH LV " + std::to_string(challengeLevel) + " (L2) | Con lai: " + formatTime(countdownSeconds) + " | Khong bao loi";
            }
            sf::Text txtStats(statsStr, font, 14); txtStats.setFillColor(sf::Color(44, 62, 80));
            txtStats.setPosition(GRID_OFFSET, BOARD_SIZE + GRID_OFFSET + 20); window.draw(txtStats);

            // Nút "<< MENU" góc phải dashboard
            window.draw(btnBackInGame);
            sf::Text txtBackInGame("<< MENU", font, 14); txtBackInGame.setFillColor(sf::Color::White);
            txtBackInGame.setPosition(btnBackInGame.getPosition().x + 40, btnBackInGame.getPosition().y + 8);
            window.draw(txtBackInGame);

            // Hướng dẫn phím tắt
            sf::Text txtGuide("Phim: 1-9 (Dien) | 0/Backspace (Xoa)", font, 14);
            txtGuide.setFillColor(sf::Color(127, 140, 141)); txtGuide.setPosition(GRID_OFFSET, BOARD_SIZE + GRID_OFFSET + 55); window.draw(txtGuide);
        }
        // ── SCREEN: THUA CUỘC (GAME_OVER) ────────────────────────────────────
        else if (mode == GameMode::GAME_OVER) {
            // Tiêu đề thông báo thua
            sf::Text t1("BAN DA THUA CUOC!", font, 34); t1.setFillColor(sf::Color(231, 76, 60)); t1.setStyle(sf::Text::Bold);
            t1.setPosition(WINDOW_WIDTH/2 - t1.getGlobalBounds().width/2, 80); window.draw(t1);

            // Lý do thua: hết giờ hay sai quá nhiều
            std::string reason = isGameOverReasonTime ? "Ly do: Het thoi gian quy dinh!" : "Ly do: Vuot qua so luot loi cho phep!";
            sf::Text t2(reason, font, 20); t2.setFillColor(sf::Color(44, 62, 80)); t2.setPosition(WINDOW_WIDTH/2 - t2.getGlobalBounds().width/2, 140); window.draw(t2);

            if (!revealAnswer) {
                // Chưa xem đáp án → hiện 2 nút để người chơi lựa chọn
                window.draw(btnViewAnswer);
                sf::Text tv("XEM DAP AN", font, 16); tv.setStyle(sf::Text::Bold); tv.setFillColor(sf::Color::White);
                tv.setPosition(btnViewAnswer.getPosition().x + btnViewAnswer.getSize().x/2 - tv.getGlobalBounds().width/2, btnViewAnswer.getPosition().y + 14);
                window.draw(tv);

                window.draw(btnBackFromGameOver);
                sf::Text tb("VE MENU CHINH", font, 16); tb.setStyle(sf::Text::Bold); tb.setFillColor(sf::Color::White);
                tb.setPosition(btnBackFromGameOver.getPosition().x + btnBackFromGameOver.getSize().x/2 - tb.getGlobalBounds().width/2, btnBackFromGameOver.getPosition().y + 14);
                window.draw(tb);
            } else {
                // Đã bấm "Xem đáp án": vẽ bảng đáp án thu nhỏ (ô 50px) cho vừa màn hình
                const float ansCell    = 50.0f;
                const float ansOffsetX = WINDOW_WIDTH/2 - (ansCell * 9) / 2.0f; // Căn giữa ngang
                const float ansOffsetY = 200.0f; // Bắt đầu từ y=200 để không đè lên thông báo thua

                for (int r = 0; r < 9; r++) {
                    for (int c = 0; c < 9; c++) {
                        int val = board.solution[r][c]; // Luôn lấy từ đáp án đúng
                        sf::Text text(std::to_string(val), font, 22);
                        // Phân biệt màu: số đề bài (đen) vs số đáp án bổ sung (cam)
                        text.setFillColor(board.isFixed(r, c) ? sf::Color(44, 62, 80) : sf::Color(230, 126, 34));
                        sf::FloatRect textRect = text.getLocalBounds();
                        text.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                        text.setPosition(ansOffsetX + c * ansCell + ansCell / 2.0f, ansOffsetY + r * ansCell + ansCell / 2.0f);
                        window.draw(text);
                    }
                }

                // Vẽ lưới mỏng cho bảng đáp án thu nhỏ
                for (int i = 0; i <= 9; i++) {
                    sf::RectangleShape vL(sf::Vector2f(i % 3 == 0 ? 2.f : 1.f, ansCell * 9));
                    vL.setFillColor(i % 3 == 0 ? sf::Color(44,62,80) : sf::Color(200,200,200));
                    vL.setPosition(ansOffsetX + i * ansCell, ansOffsetY);
                    window.draw(vL);
                    sf::RectangleShape hL(sf::Vector2f(ansCell * 9, i % 3 == 0 ? 2.f : 1.f));
                    hL.setFillColor(i % 3 == 0 ? sf::Color(44,62,80) : sf::Color(200,200,200));
                    hL.setPosition(ansOffsetX, ansOffsetY + i * ansCell);
                    window.draw(hL);
                }

                // Chú thích màu sắc đáp án
                sf::Text tHint("Mau cam la dap an dung. Bam nut duoi de ve Menu", font, 14);
                tHint.setFillColor(sf::Color(127, 140, 141));
                tHint.setPosition(WINDOW_WIDTH/2 - tHint.getGlobalBounds().width/2, ansOffsetY + ansCell * 9 + 15);
                window.draw(tHint);

                // Nút về Menu (cố định ở đáy cửa sổ, không đè lên bảng giải)
                window.draw(btnBackAfterReveal);
                sf::Text tb2("VE MENU CHINH", font, 16); tb2.setStyle(sf::Text::Bold); tb2.setFillColor(sf::Color::White);
                tb2.setPosition(btnBackAfterReveal.getPosition().x + btnBackAfterReveal.getSize().x/2 - tb2.getGlobalBounds().width/2, btnBackAfterReveal.getPosition().y + 10);
                window.draw(tb2);
            }
        }
        // ── SCREEN: CHIẾN THẮNG (VICTORY) ────────────────────────────────────
        else if (mode == GameMode::VICTORY) {
            // Thông báo chiến thắng
            sf::Text t1("CHIEN THANG!", font, 36); t1.setFillColor(sf::Color(46, 204, 113)); t1.setStyle(sf::Text::Bold);
            t1.setPosition(WINDOW_WIDTH/2 - t1.getGlobalBounds().width/2, 200); window.draw(t1);

            // Thống kê ván thắng: thời gian và tổng lỗi
            sf::Text t2("Thoi gian: " + formatTime(elapsedSeconds) + " | Tong loi sai: " + std::to_string(wrongMoves), font, 20);
            t2.setFillColor(sf::Color(44, 62, 80)); t2.setPosition(WINDOW_WIDTH/2 - t2.getGlobalBounds().width/2, 280); window.draw(t2);

            // Hướng dẫn thoát
            sf::Text t3("Bam nut bat ky de ve Menu", font, 16); t3.setFillColor(sf::Color(149, 165, 166)); t3.setPosition(WINDOW_WIDTH/2 - t3.getGlobalBounds().width/2, 450); window.draw(t3);
        }

        // Đẩy toàn bộ nội dung đã vẽ lên màn hình (double buffering)
        window.display();

    } // Kết thúc Game Loop

    return 0; // Chương trình kết thúc bình thường
}

#include <IRremote.hpp>        // Bao gồm thư viện IRremote phiên bản 4.x để xử lý tín hiệu hồng ngoại
#include <Adafruit_NeoPixel.h> // Bao gồm thư viện Adafruit NeoPixel để điều khiển dải LED NeoPixel
#include <math.h>              // Bao gồm thư viện math để sử dụng các hàm toán học như sin, pow,...

#define IR_RECEIVE_PIN 7 // Định nghĩa chân số 7 làm chân nhận tín hiệu từ module IR (1838T – MH-R38)
#define LED_PIN 3        // Định nghĩa chân số 3 làm chân gửi dữ liệu đến dải LED NeoPixel
#define LED_COUNT 12     // Định nghĩa số lượng LED trong dải là 12

#define AUDIO_PIN A0           // Định nghĩa chân analog A0 để nhận tín hiệu âm thanh từ micro hoặc nguồn âm thanh
#define KNOB_PIN A1            // Định nghĩa chân analog A1 để đọc giá trị từ núm điều chỉnh (potentiometer)
#define BUTTON_1 6             // Định nghĩa chân số 6 cho nút nhấn 1
#define BUTTON_2 5             // Định nghĩa chân số 5 cho nút nhấn 2
#define BUTTON_3 4             // Định nghĩa chân số 4 cho nút nhấn 3
#define SAMPLE_WINDOW 10       // Định nghĩa thời gian lấy mẫu âm thanh là 10ms
#define INPUT_FLOOR 50         // Định nghĩa ngưỡng dưới của tín hiệu âm thanh là 50
#define INPUT_CEILING 400      // Định nghĩa ngưỡng trên của tín hiệu âm thanh là 400
#define PEAK_HANG 15           // Định nghĩa thời gian giữ đỉnh cho hiệu ứng VU là 15 chu kỳ
#define PEAK_FALL 6            // Định nghĩa tốc độ giảm đỉnh cho hiệu ứng VU là 6 chu kỳ
#define LED_TOTAL LED_COUNT    // Gán tổng số LED bằng LED_COUNT (12) để sử dụng trong tính toán
#define LED_HALF LED_COUNT / 2 // Tính số LED một nửa dải (6) để sử dụng trong các hiệu ứng đối xứng
#define VISUALS 6              // Định nghĩa số lượng hiệu ứng visual là 6

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800); // Khởi tạo đối tượng strip để điều khiển dải LED với 12 LED, chân 3, định dạng màu GRB, tần số 800kHz

bool ledOn = false;            // Biến toàn cục theo dõi trạng thái bật/tắt của LED (false: tắt, true: bật)
uint8_t brightness = 150;      // Biến toàn cục lưu độ sáng của LED, giá trị ban đầu là 150 (0-255)
bool animationMode = false;    // Biến toàn cục kiểm soát chế độ animation (false: tĩnh, true: động)
bool animationPlaying = false; // Biến toàn cục kiểm soát trạng thái phát animation (false: dừng, true: chạy)
int currentEffect = -1;        // Biến toàn cục lưu hiệu ứng hiện tại (-1: không có hiệu ứng nào)
int previousEffect = -1;       // Biến toàn cục lưu hiệu ứng trước đó để quay lại nếu cần
int colorShift = 0;            // Biến toàn cục lưu giá trị dịch chuyển màu (không sử dụng trong code hiện tại)
const int numColors = 7;       // Định nghĩa hằng số số lượng màu cố định là 7
uint32_t colorSequence[numColors] = {
    // Mảng lưu trữ chuỗi màu cố định cho các chế độ màu
    0,                        // Giá trị 0 đại diện cho chế độ cầu vồng (rainbow mode)
    strip.Color(255, 0, 0),   // Màu đỏ (RGB: 255, 0, 0)
    strip.Color(0, 255, 0),   // Màu xanh lá (RGB: 0, 255, 0)
    strip.Color(0, 0, 255),   // Màu xanh dương (RGB: 0, 0, 255)
    strip.Color(255, 255, 0), // Màu vàng (RGB: 255, 255, 0)
    strip.Color(0, 255, 255), // Màu cyan (RGB: 0, 255, 255)
    strip.Color(255, 0, 255)  // Màu magenta (RGB: 255, 0, 255)
};
int currentColorIndex = 0; // Biến toàn cục lưu chỉ số màu hiện tại trong mảng colorSequence (0-6)
uint32_t currentColor = 0; // Biến toàn cục lưu giá trị màu hiện tại (0 là cầu vồng, các giá trị khác là màu cố định)

bool testEffectActive = false;     // Biến toàn cục kiểm soát trạng thái hiệu ứng test (false: không chạy, true: chạy)
unsigned long testEffectStart = 0; // Biến toàn cục lưu thời điểm bắt đầu hiệu ứng test (đơn vị: ms)
unsigned long lastTestUpdate = 0;  // Biến toàn cục lưu thời điểm cập nhật cuối cùng của hiệu ứng test (đơn vị: ms)
int testEffectIndex = 0;           // Biến toàn cục lưu chỉ số màu hiện tại trong mảng testColors (0-3)
uint32_t testColors[4] = {
    // Mảng lưu trữ 4 màu cho hiệu ứng test
    strip.Color(255, 255, 255), // Màu trắng (RGB: 255, 255, 255)
    strip.Color(255, 0, 0),     // Màu đỏ (RGB: 255, 0, 0)
    strip.Color(0, 255, 0),     // Màu xanh lá (RGB: 0, 255, 0)
    strip.Color(0, 0, 255)      // Màu xanh dương (RGB: 0, 0, 255)
};

uint16_t gradient = 0;                                     // Biến toàn cục lưu giá trị gradient để tạo hiệu ứng chuyển màu trong code 2
uint16_t thresholds[] = {1529, 1019, 764, 764, 764, 1274}; // Mảng lưu ngưỡng gradient cho từng palette (6 giá trị)
uint8_t palette = 0;                                       // Biến toàn cục lưu chỉ số palette màu hiện tại (0-5)
uint8_t visual = 0;                                        // Biến toàn cục lưu chỉ số hiệu ứng visual hiện tại (0-6)
uint8_t volume = 0;                                        // Biến toàn cục lưu giá trị âm lượng hiện tại từ tín hiệu âm thanh
uint8_t last = 0;                                          // Biến toàn cục lưu giá trị âm lượng trước đó để so sánh
float maxVol = 1;                                          // Biến toàn cục lưu giá trị âm lượng tối đa để chuẩn hóa
float knob = 1023.0;                                       // Biến toàn cục lưu giá trị từ núm điều chỉnh (khởi tạo là 1023, nhưng sau được gán lại là 0.9)
float avgBump = 0;                                         // Biến toàn cục lưu trung bình các lần "bump" (tăng đột ngột âm lượng)
float avgVol = 0;                                          // Biến toàn cục lưu trung bình âm lượng để chuẩn hóa tín hiệu
float shuffleTime = 0;                                     // Biến toàn cục lưu thời điểm cuối cùng chuyển đổi palette hoặc visual (đơn vị: giây)
bool shuffle = true;                                       // Biến toàn cục kiểm soát chế độ tự động chuyển đổi palette/visual (true: bật, false: tắt)
bool bump = false;                                         // Biến toàn cục kiểm soát trạng thái "bump" (true: có tăng đột ngột, false: không)
int8_t pos[LED_COUNT] = {-2};                              // Mảng lưu vị trí của các "chấm" trong hiệu ứng Traffic, khởi tạo tất cả là -2 (ngoài phạm vi)
uint8_t rgb[LED_COUNT][3] = {0};                           // Mảng lưu giá trị RGB cho từng LED trong hiệu ứng Traffic, khởi tạo là 0
bool left = false;                                         // Biến toàn cục kiểm soát hướng di chuyển trong hiệu ứng PaletteDance (false: phải, true: trái)
int8_t dotPos = 0;                                         // Biến toàn cục lưu vị trí chấm trong hiệu ứng PaletteDance
float timeBump = 0;                                        // Biến toàn cục lưu thời điểm xảy ra "bump" cuối cùng (đơn vị: giây)
float avgTime = 0;                                         // Biến toàn cục lưu trung bình thời gian giữa các "bump"
byte peak = 20;                                            // Biến toàn cục lưu giá trị đỉnh trong hiệu ứng VU (khởi tạo là 20, nhưng sẽ thay đổi)
byte dotCount = 0;                                         // Biến toàn cục đếm số chu kỳ để giảm đỉnh trong hiệu ứng VU
byte dotHangCount = 0;                                     // Biến toàn cục đếm thời gian giữ đỉnh trong hiệu ứng VU
byte volCount = 0;                                         // Biến toàn cục không sử dụng trong code hiện tại (có thể là dư thừa)
unsigned int sample;                                       // Biến toàn cục lưu giá trị mẫu âm thanh từ chân AUDIO_PIN

bool mode = true; // Biến toàn cục kiểm soát chế độ hoạt động (true: code 1 - IR, false: code 2 - âm thanh)

uint32_t Wheel(byte pos)
{                    // Hàm tạo màu dựa trên vị trí trên bánh xe màu (color wheel), nhận tham số pos (0-255)
    pos = 255 - pos; // Đảo ngược giá trị pos để tạo hiệu ứng màu vòng tròn
    if (pos < 85)
    {                                                  // Nếu pos < 85, tạo màu từ đỏ sang tím
        return strip.Color(255 - pos * 3, 0, pos * 3); // Tính toán RGB: giảm đỏ, tăng tím
    }
    if (pos < 170)
    {                                                  // Nếu pos từ 85 đến 169, tạo màu từ tím sang xanh lá
        pos -= 85;                                     // Điều chỉnh pos để tính toán trong phạm vi mới
        return strip.Color(0, pos * 3, 255 - pos * 3); // Tính toán RGB: tăng xanh lá, giảm tím
    }
    pos -= 170;                                    // Nếu pos từ 170 đến 255, tạo màu từ xanh lá sang đỏ
    return strip.Color(pos * 3, 255 - pos * 3, 0); // Tính toán RGB: tăng đỏ, giảm xanh lá
}

void displayRainbowBase()
{ // Hàm hiển thị hiệu ứng cầu vồng cơ bản trên toàn bộ dải LED
    for (int i = 0; i < LED_COUNT; i++)
    {                                                    // Lặp qua từng LED
        uint8_t wheelPos = (i * 256 / LED_COUNT) & 0xFF; // Tính vị trí trên bánh xe màu cho mỗi LED (0-255)
        strip.setPixelColor(i, Wheel(wheelPos));         // Đặt màu cho LED tại vị trí i dựa trên hàm Wheel
    }
    strip.show(); // Hiển thị màu lên dải LED
}

void setAllLEDs(uint32_t color)
{ // Hàm đặt tất cả LED thành một màu cố định
    for (int i = 0; i < LED_COUNT; i++)
    {                                  // Lặp qua từng LED
        strip.setPixelColor(i, color); // Đặt màu cho LED tại vị trí i
    }
    strip.show(); // Hiển thị màu lên dải LED
}

void turnOffLEDs()
{                  // Hàm tắt toàn bộ LED
    setAllLEDs(0); // Gọi hàm setAllLEDs với màu đen (0) để tắt LED
}

void updateStaticDisplay()
{ // Hàm cập nhật hiển thị tĩnh (không animation)
    if (!ledOn)
    {                  // Nếu LED đang tắt
        turnOffLEDs(); // Tắt toàn bộ LED
        return;        // Thoát hàm
    }
    if (currentColorIndex == 0)
    {                         // Nếu chỉ số màu là 0 (chế độ cầu vồng)
        displayRainbowBase(); // Hiển thị hiệu ứng cầu vồng
    }
    else
    {                             // Nếu chỉ số màu khác 0 (màu cố định)
        setAllLEDs(currentColor); // Đặt toàn bộ LED thành màu hiện tại
    }
}

float fscale(float originalMin, float originalMax, float newBegin, float newEnd, float inputValue, float curve)
{                               // Hàm chuyển đổi giá trị với đường cong
    float OriginalRange = 0;    // Biến lưu phạm vi ban đầu của giá trị đầu vào
    float NewRange = 0;         // Biến lưu phạm vi mới của giá trị đầu ra
    float zeroRefCurVal = 0;    // Biến lưu giá trị đầu vào đã được chuẩn hóa về 0
    float normalizedCurVal = 0; // Biến lưu giá trị đã được chuẩn hóa trong khoảng 0-1
    float rangedValue = 0;      // Biến lưu giá trị đầu ra cuối cùng
    boolean invFlag = 0;        // Cờ kiểm soát hướng của phạm vi mới (0: thuận, 1: ngược)

    if (curve > 10)
        curve = 10; // Giới hạn giá trị curve tối đa là 10
    if (curve < -10)
        curve = -10;        // Giới hạn giá trị curve tối thiểu là -10
    curve = (curve * -.1);  // Điều chỉnh curve để phù hợp với hàm pow (đảo dấu và giảm 10 lần)
    curve = pow(10, curve); // Tính lũy thừa cơ số 10 của curve để tạo đường cong

    if (inputValue < originalMin)
        inputValue = originalMin; // Giới hạn giá trị đầu vào không nhỏ hơn originalMin
    if (inputValue > originalMax)
        inputValue = originalMax; // Giới hạn giá trị đầu vào không lớn hơn originalMax

    OriginalRange = originalMax - originalMin; // Tính phạm vi ban đầu
    if (newEnd > newBegin)
        NewRange = newEnd - newBegin; // Nếu phạm vi mới thuận, tính NewRange
    else
    {                                 // Nếu phạm vi mới ngược
        NewRange = newBegin - newEnd; // Tính NewRange theo hướng ngược
        invFlag = 1;                  // Đặt cờ đảo ngược
    }

    zeroRefCurVal = inputValue - originalMin;         // Chuẩn hóa giá trị đầu vào về 0
    normalizedCurVal = zeroRefCurVal / OriginalRange; // Chuẩn hóa giá trị vào khoảng 0-1
    if (originalMin > originalMax)
        return 0; // Nếu phạm vi ban đầu không hợp lệ, trả về 0

    if (invFlag == 0)
    {                                                                       // Nếu phạm vi mới thuận
        rangedValue = (pow(normalizedCurVal, curve) * NewRange) + newBegin; // Tính giá trị đầu ra với đường cong
    }
    else
    {                                                                       // Nếu phạm vi mới ngược
        rangedValue = newBegin - (pow(normalizedCurVal, curve) * NewRange); // Tính giá trị đầu ra đảo ngược
    }
    return rangedValue; // Trả về giá trị đã chuyển đổi
}

void drawLine(uint8_t from, uint8_t to, uint32_t c)
{                     // Hàm vẽ một đường từ vị trí 'from' đến 'to' với màu 'c'
    uint8_t fromTemp; // Biến tạm để hoán đổi giá trị nếu from > to
    if (from > to)
    {                    // Nếu vị trí bắt đầu lớn hơn vị trí kết thúc
        fromTemp = from; // Lưu giá trị from vào biến tạm
        from = to;       // Gán from bằng to
        to = fromTemp;   // Gán to bằng giá trị ban đầu của from
    }
    for (int i = from; i <= to; i++)
    {                              // Lặp từ vị trí from đến to
        strip.setPixelColor(i, c); // Đặt màu c cho LED tại vị trí i
    }
}

void fade(float damper)
{ // Hàm làm mờ màu của tất cả LED với hệ số damper (0-1)
    for (int i = 0; i < strip.numPixels(); i++)
    {                                          // Lặp qua tất cả LED
        uint32_t col = strip.getPixelColor(i); // Lấy màu hiện tại của LED tại vị trí i
        if (col == 0)
            continue;    // Nếu màu là đen (0), bỏ qua LED này
        float colors[3]; // Mảng lưu giá trị RGB sau khi làm mờ
        for (int j = 0; j < 3; j++)
            colors[j] = split(col, j) * damper;                               // Tính giá trị RGB mới bằng cách nhân với damper
        strip.setPixelColor(i, strip.Color(colors[0], colors[1], colors[2])); // Đặt màu mới cho LED
    }
}

uint8_t split(uint32_t color, uint8_t i)
{ // Hàm tách thành phần màu (R, G, B) từ giá trị uint32_t
    if (i == 0)
        return color >> 16; // Nếu i = 0, trả về thành phần đỏ (dịch phải 16 bit)
    if (i == 1)
        return color >> 8; // Nếu i = 1, trả về thành phần xanh lá (dịch phải 8 bit)
    if (i == 2)
        return color >> 0; // Nếu i = 2, trả về thành phần xanh dương (không dịch)
    return -1;             // Nếu i không hợp lệ, trả về -1
}

uint32_t ColorPalette(float num)
{ // Hàm chọn màu từ palette hiện tại dựa trên giá trị num
    switch (palette)
    { // Chọn palette dựa trên giá trị của biến palette
    case 0:
        return (num < 0) ? Rainbow(gradient) : Rainbow(num); // Palette 0: cầu vồng
    case 1:
        return (num < 0) ? Sunset(gradient) : Sunset(num); // Palette 1: hoàng hôn
    case 2:
        return (num < 0) ? Ocean(gradient) : Ocean(num); // Palette 2: đại dương
    case 3:
        return (num < 0) ? PinaColada(gradient) : PinaColada(num); // Palette 3: Pina Colada
    case 4:
        return (num < 0) ? Sulfur(gradient) : Sulfur(num); // Palette 4: lưu huỳnh
    case 5:
        return (num < 0) ? NoGreen(gradient) : NoGreen(num); // Palette 5: không xanh lá
    default:
        return Rainbow(gradient); // Mặc định: trả về cầu vồng nếu palette không hợp lệ
    }
}

uint32_t Rainbow(unsigned int i)
{ // Hàm tạo màu cầu vồng dựa trên giá trị i
    if (i > 1529)
        return Rainbow(i % 1530); // Nếu i vượt ngưỡng, quay lại vòng mới
    if (i > 1274)
        return strip.Color(255, 0, 255 - (i % 255)); // Từ tím sang magenta
    if (i > 1019)
        return strip.Color((i % 255), 0, 255); // Từ magenta sang đỏ
    if (i > 764)
        return strip.Color(0, 255 - (i % 255), 255); // Từ đỏ sang xanh dương
    if (i > 509)
        return strip.Color(0, 255, (i % 255)); // Từ xanh dương sang xanh lá
    if (i > 255)
        return strip.Color(255 - (i % 255), 255, 0); // Từ xanh lá sang vàng
    return strip.Color(255, i, 0);                   // Từ vàng sang đỏ
}

uint32_t Sunset(unsigned int i)
{ // Hàm tạo màu hoàng hôn dựa trên giá trị i
    if (i > 1019)
        return Sunset(i % 1020); // Nếu i vượt ngưỡng, quay lại vòng mới
    if (i > 764)
        return strip.Color((i % 255), 0, 255 - (i % 255)); // Từ tím sang đỏ
    if (i > 509)
        return strip.Color(255 - (i % 255), 0, 255); // Từ đỏ sang tím nhạt
    if (i > 255)
        return strip.Color(255, 128 - (i % 255) / 2, (i % 255)); // Từ tím nhạt sang cam
    return strip.Color(255, i / 2, 0);                           // Từ cam sang đỏ nhạt
}

uint32_t Ocean(unsigned int i)
{ // Hàm tạo màu đại dương dựa trên giá trị i
    if (i > 764)
        return Ocean(i % 765); // Nếu i vượt ngưỡng, quay lại vòng mới
    if (i > 509)
        return strip.Color(0, i % 255, 255 - (i % 255)); // Từ xanh dương đậm sang xanh nhạt
    if (i > 255)
        return strip.Color(0, 255 - (i % 255), 255); // Từ xanh nhạt sang xanh dương
    return strip.Color(0, 255, i);                   // Từ xanh dương sang xanh nhạt hơn
}

uint32_t PinaColada(unsigned int i)
{ // Hàm tạo màu Pina Colada dựa trên giá trị i
    if (i > 764)
        return PinaColada(i % 765); // Nếu i vượt ngưỡng, quay lại vòng mới
    if (i > 509)
        return strip.Color(255 - (i % 255) / 2, (i % 255) / 2, (i % 255) / 2); // Từ xám sang hồng nhạt
    if (i > 255)
        return strip.Color(255, 255 - (i % 255), 0);               // Từ vàng sang trắng
    return strip.Color(128 + (i / 2), 128 + (i / 2), 128 - i / 2); // Từ xám nhạt sang vàng nhạt
}

uint32_t Sulfur(unsigned int i)
{ // Hàm tạo màu lưu huỳnh dựa trên giá trị i
    if (i > 764)
        return Sulfur(i % 765); // Nếu i vượt ngưỡng, quay lại vòng mới
    if (i > 509)
        return strip.Color(i % 255, 255, 255 - (i % 255)); // Từ trắng sang vàng nhạt
    if (i > 255)
        return strip.Color(0, 255, i % 255); // Từ xanh lá sang trắng
    return strip.Color(255 - i, 255, 0);     // Từ vàng sang xanh lá
}

uint32_t NoGreen(unsigned int i)
{ // Hàm tạo màu không xanh lá dựa trên giá trị i
    if (i > 1274)
        return NoGreen(i % 1275); // Nếu i vượt ngưỡng, quay lại vòng mới
    if (i > 1019)
        return strip.Color(255, 0, 255 - (i % 255)); // Từ tím sang magenta
    if (i > 764)
        return strip.Color((i % 255), 0, 255); // Từ magenta sang đỏ
    if (i > 509)
        return strip.Color(0, 255 - (i % 255), 255); // Từ đỏ sang xanh dương
    if (i > 255)
        return strip.Color(255 - (i % 255), 255, i % 255); // Từ xanh dương sang tím
    return strip.Color(255, i, 0);                         // Từ đỏ sang vàng
}

void updateEffect0()
{                                         // Hàm cập nhật hiệu ứng cầu vồng quay (Rainbow Cycle)
    static uint16_t j = 0;                // Biến tĩnh lưu giá trị dịch chuyển màu, khởi tạo là 0
    static unsigned long lastJUpdate = 0; // Biến tĩnh lưu thời điểm cập nhật j cuối cùng
    static unsigned long lastShow = 0;    // Biến tĩnh lưu thời điểm hiển thị cuối cùng
    unsigned long now = millis();         // Lấy thời gian hiện tại (ms)

    if (now - lastJUpdate >= 15)
    {                      // Nếu đã qua 15ms kể từ lần cập nhật j cuối cùng
        j++;               // Tăng giá trị j để dịch chuyển màu
        lastJUpdate = now; // Cập nhật thời điểm cuối cùng của j
    }

    if (now - lastShow >= 70)
    { // Nếu đã qua 70ms kể từ lần hiển thị cuối cùng
        for (int i = 0; i < LED_COUNT; i++)
        { // Lặp qua từng LED
            if (currentColor == 0)
            {                                                     // Nếu ở chế độ cầu vồng
                uint8_t pos = ((i * 256 / LED_COUNT) + j) & 0xFF; // Tính vị trí màu trên bánh xe, thêm j để quay
                strip.setPixelColor(i, Wheel(pos));               // Đặt màu cho LED dựa trên hàm Wheel
            }
            else
            {                                         // Nếu ở chế độ màu cố định
                strip.setPixelColor(i, currentColor); // Đặt màu cố định cho LED
            }
        }
        strip.show();   // Hiển thị màu lên dải LED
        lastShow = now; // Cập nhật thời điểm hiển thị cuối cùng
    }
    IrReceiver.resume(); // Tiếp tục nhận tín hiệu IR sau khi xử lý
}

void updateEffect1()
{                                      // Hàm cập nhật hiệu ứng thở (Breathing effect)
    static unsigned long lastShow = 0; // Biến tĩnh lưu thời điểm hiển thị cuối cùng
    unsigned long now = millis();      // Lấy thời gian hiện tại (ms)

    float phase = (now % 2000) / 2000.0 * 2 * PI; // Tính pha của sóng sin trong chu kỳ 2000ms
    float factor = (sin(phase) + 1) / 2.0;        // Chuẩn hóa sóng sin từ -1..1 thành 0..1 để điều chỉnh độ sáng

    for (int i = 0; i < LED_COUNT; i++)
    { // Lặp qua từng LED
        if (currentColor == 0)
        {                                                    // Nếu ở chế độ cầu vồng
            uint8_t basePos = (i * 256 / LED_COUNT) & 0xFF;  // Tính vị trí màu cơ bản trên bánh xe
            uint32_t baseColor = Wheel(basePos);             // Lấy màu cơ bản từ hàm Wheel
            uint8_t r = ((baseColor >> 16) & 0xFF) * factor; // Điều chỉnh đỏ theo factor
            uint8_t g = ((baseColor >> 8) & 0xFF) * factor;  // Điều chỉnh xanh lá theo factor
            uint8_t b = (baseColor & 0xFF) * factor;         // Điều chỉnh xanh dương theo factor
            strip.setPixelColor(i, strip.Color(r, g, b));    // Đặt màu đã điều chỉnh cho LED
        }
        else
        {                                                         // Nếu ở chế độ màu cố định
            uint8_t r = (((currentColor >> 16) & 0xFF)) * factor; // Điều chỉnh đỏ theo factor
            uint8_t g = (((currentColor >> 8) & 0xFF)) * factor;  // Điều chỉnh xanh lá theo factor
            uint8_t b = ((currentColor & 0xFF)) * factor;         // Điều chỉnh xanh dương theo factor
            strip.setPixelColor(i, strip.Color(r, g, b));         // Đặt màu đã điều chỉnh cho LED
        }
    }

    if (now - lastShow >= 30)
    {                   // Nếu đã qua 30ms kể từ lần hiển thị cuối cùng
        strip.show();   // Hiển thị màu lên dải LED
        lastShow = now; // Cập nhật thời điểm hiển thị cuối cùng
    }
}

void updateEffect2()
{                                        // Hàm cập nhật hiệu ứng nhấp nháy xen kẽ (Alternating flashing)
    static bool state = false;           // Biến tĩnh lưu trạng thái nhấp nháy (true: bật, false: tắt)
    static unsigned long lastToggle = 0; // Biến tĩnh lưu thời điểm chuyển đổi trạng thái cuối cùng
    static unsigned long lastShow = 0;   // Biến tĩnh lưu thời điểm hiển thị cuối cùng
    unsigned long now = millis();        // Lấy thời gian hiện tại (ms)

    if (now - lastToggle >= 350)
    {                     // Nếu đã qua 350ms kể từ lần chuyển đổi cuối cùng
        state = !state;   // Đảo trạng thái nhấp nháy
        lastToggle = now; // Cập nhật thời điểm chuyển đổi cuối cùng

        for (int i = 0; i < LED_COUNT; i++)
        { // Lặp qua từng LED
            if ((i % 2 == 0 && state) || (i % 2 == 1 && !state))
            { // Nếu LED chẵn và bật hoặc LED lẻ và tắt
                if (currentColor == 0)
                {                                               // Nếu ở chế độ cầu vồng
                    uint8_t pos = (i * 256 / LED_COUNT) & 0xFF; // Tính vị trí màu trên bánh xe
                    strip.setPixelColor(i, Wheel(pos));         // Đặt màu cầu vồng cho LED
                }
                else
                {                                         // Nếu ở chế độ màu cố định
                    strip.setPixelColor(i, currentColor); // Đặt màu cố định cho LED
                }
            }
            else
            {                              // Nếu LED không được bật
                strip.setPixelColor(i, 0); // Tắt LED
            }
        }
    }

    if (now - lastShow >= 70)
    {                   // Nếu đã qua 70ms kể từ lần hiển thị cuối cùng
        strip.show();   // Hiển thị màu lên dải LED
        lastShow = now; // Cập nhật thời điểm hiển thị cuối cùng
    }

    IrReceiver.resume(); // Tiếp tục nhận tín hiệu IR sau khi xử lý
}

void updateEffect3()
{                                        // Hàm cập nhật hiệu ứng đuổi theo (Moving trail effect)
    static int pos = 0;                  // Biến tĩnh lưu vị trí hiện tại của "đuôi" di chuyển
    static unsigned long lastUpdate = 0; // Biến tĩnh lưu thời điểm cập nhật cuối cùng
    unsigned long now = millis();        // Lấy thời gian hiện tại (ms)
    if (now - lastUpdate >= 50)
    { // Nếu đã qua 50ms kể từ lần cập nhật cuối cùng
        for (int i = 0; i < LED_COUNT; i++)
        {                                                 // Lặp qua từng LED
            uint32_t col = strip.getPixelColor(i);        // Lấy màu hiện tại của LED
            uint8_t r = ((col >> 16) & 0xFF) * 0.4;       // Giảm độ sáng đỏ xuống 40%
            uint8_t g = ((col >> 8) & 0xFF) * 0.4;        // Giảm độ sáng xanh lá xuống 40%
            uint8_t b = (col & 0xFF) * 0.65;              // Giảm độ sáng xanh dương xuống 65%
            strip.setPixelColor(i, strip.Color(r, g, b)); // Đặt màu đã giảm sáng cho LED
        }
        if (currentColor == 0)
        {                                                           // Nếu ở chế độ cầu vồng
            uint8_t colorWheelPos = (pos * 256 / LED_COUNT) & 0xFF; // Tính vị trí màu trên bánh xe
            strip.setPixelColor(pos, Wheel(colorWheelPos));         // Đặt màu cầu vồng cho LED tại vị trí pos
        }
        else
        {                                           // Nếu ở chế độ màu cố định
            strip.setPixelColor(pos, currentColor); // Đặt màu cố định cho LED tại vị trí pos
        }

        strip.show();                // Hiển thị màu lên dải LED
        pos = (pos + 1) % LED_COUNT; // Tăng vị trí pos, quay lại 0 nếu vượt quá LED_COUNT
        lastUpdate = now;            // Cập nhật thời điểm cuối cùng
    }
    IrReceiver.resume(); // Tiếp tục nhận tín hiệu IR sau khi xử lý
}

void updateEffect4()
{                                        // Hàm cập nhật hiệu ứng sóng (Wave effect)
    static bool phase = false;           // Biến tĩnh lưu pha của sóng (false: lan ra, true: thu vào)
    static int currentLED = 0;           // Biến tĩnh lưu vị trí LED hiện tại trong sóng
    static unsigned long lastUpdate = 0; // Biến tĩnh lưu thời điểm cập nhật cuối cùng
    unsigned long now = millis();        // Lấy thời gian hiện tại (ms)
    const unsigned long interval = 100;  // Định nghĩa khoảng thời gian cập nhật là 100ms

    if (now - lastUpdate >= interval)
    { // Nếu đã qua 100ms kể từ lần cập nhật cuối cùng
        if (!phase)
        { // Nếu đang ở pha lan ra
            if (currentColor == 0)
            {                                                               // Nếu ở chế độ cầu vồng
                uint8_t colorIndex = (currentLED * 256 / LED_COUNT) & 0xFF; // Tính vị trí màu trên bánh xe
                strip.setPixelColor(currentLED, Wheel(colorIndex));         // Đặt màu cầu vồng cho LED
            }
            else
            {                                                  // Nếu ở chế độ màu cố định
                strip.setPixelColor(currentLED, currentColor); // Đặt màu cố định cho LED
            }
            currentLED++; // Tăng vị trí LED hiện tại
            if (currentLED >= LED_COUNT)
            {                   // Nếu đã đến cuối dải
                phase = true;   // Chuyển sang pha thu vào
                currentLED = 0; // Đặt lại vị trí về 0
            }
        }
        else
        {                                       // Nếu đang ở pha thu vào
            strip.setPixelColor(currentLED, 0); // Tắt LED tại vị trí hiện tại
            currentLED++;                       // Tăng vị trí LED hiện tại
            if (currentLED >= LED_COUNT)
            {                   // Nếu đã đến cuối dải
                phase = false;  // Chuyển sang pha lan ra
                currentLED = 0; // Đặt lại vị trí về 0
            }
        }
        strip.show();     // Hiển thị màu lên dải LED
        lastUpdate = now; // Cập nhật thời điểm cuối cùng
    }
}

void updateEffect5()
{                                        // Hàm cập nhật hiệu ứng lấp lánh (Sparkle effect)
    static unsigned long lastUpdate = 0; // Biến tĩnh lưu thời điểm cập nhật cuối cùng
    unsigned long now = millis();        // Lấy thời gian hiện tại (ms)

    if (now - lastUpdate >= 120)
    { // Nếu đã qua 120ms kể từ lần cập nhật cuối cùng
        for (int i = 0; i < LED_COUNT; i++)
        { // Lặp qua từng LED
            if (random(100) < 40)
            { // Nếu ngẫu nhiên nhỏ hơn 40 (40% cơ hội)
                if (currentColor == 0)
                {                                                 // Nếu ở chế độ cầu vồng
                    uint8_t randomColorPos = random(256);         // Chọn ngẫu nhiên vị trí trên bánh xe màu
                    uint32_t randomColor = Wheel(randomColorPos); // Lấy màu ngẫu nhiên từ hàm Wheel
                    strip.setPixelColor(i, randomColor);          // Đặt màu ngẫu nhiên cho LED
                }
                else
                {                                         // Nếu ở chế độ màu cố định
                    strip.setPixelColor(i, currentColor); // Đặt màu cố định cho LED
                }
            }
            else
            {                              // Nếu không lấp lánh
                strip.setPixelColor(i, 0); // Tắt LED
            }
        }
        strip.show();     // Hiển thị màu lên dải LED
        lastUpdate = now; // Cập nhật thời điểm cuối cùng
    }
}

void updateEffect6()
{                                        // Hàm cập nhật hiệu ứng pháo hoa (Fireworks effect)
    static unsigned long lastUpdate = 0; // Biến tĩnh lưu thời điểm cập nhật cuối cùng
    static int step = 0;                 // Biến tĩnh lưu bước hiện tại của pháo hoa (khoảng cách từ giữa)
    static int direction = 1;            // Biến tĩnh lưu hướng di chuyển (1: ra ngoài, -1: vào trong)
    unsigned long now = millis();        // Lấy thời gian hiện tại (ms)

    if (now - lastUpdate >= 100)
    { // Nếu đã qua 100ms kể từ lần cập nhật cuối cùng
        for (int i = 0; i < LED_COUNT; i++)
        {                                                 // Lặp qua từng LED
            uint32_t col = strip.getPixelColor(i);        // Lấy màu hiện tại của LED
            uint8_t r = ((col >> 16) & 0xFF) * 0.325;     // Giảm độ sáng đỏ xuống 32.5%
            uint8_t g = ((col >> 8) & 0xFF) * 0.325;      // Giảm độ sáng xanh lá xuống 32.5%
            uint8_t b = (col & 0xFF) * 0.325;             // Giảm độ sáng xanh dương xuống 32.5%
            strip.setPixelColor(i, strip.Color(r, g, b)); // Đặt màu đã giảm sáng cho LED
        }

        int centerLeft = LED_COUNT / 2 - 1; // Tính vị trí trung tâm bên trái (5)
        int centerRight = LED_COUNT / 2;    // Tính vị trí trung tâm bên phải (6)

        uint32_t color; // Biến lưu màu sẽ sử dụng cho pháo hoa
        if (currentColor == 0)
        {                                                             // Nếu ở chế độ cầu vồng
            uint8_t colorPos = (step * 256 / (LED_COUNT / 2)) & 0xFF; // Tính vị trí màu trên bánh xe dựa trên bước
            color = Wheel(colorPos);                                  // Lấy màu từ hàm Wheel
        }
        else
        {                         // Nếu ở chế độ màu cố định
            color = currentColor; // Sử dụng màu cố định
        }

        if (step == 0)
        {                                            // Nếu ở bước đầu tiên
            strip.setPixelColor(centerLeft, color);  // Đặt màu cho trung tâm bên trái
            strip.setPixelColor(centerRight, color); // Đặt màu cho trung tâm bên phải
        }
        else
        {                                   // Nếu ở các bước tiếp theo
            int left = centerLeft - step;   // Tính vị trí bên trái
            int right = centerRight + step; // Tính vị trí bên phải
            if (left >= 0)
            {                                     // Nếu vị trí bên trái hợp lệ
                strip.setPixelColor(left, color); // Đặt màu cho vị trí bên trái
            }
            if (right < LED_COUNT)
            {                                      // Nếu vị trí bên phải hợp lệ
                strip.setPixelColor(right, color); // Đặt màu cho vị trí bên phải
            }
        }

        strip.show(); // Hiển thị màu lên dải LED

        step += direction; // Tăng hoặc giảm bước tùy theo hướng
        if (step >= centerLeft + 1)
        {                          // Nếu bước vượt quá giới hạn ngoài
            step = centerLeft + 1; // Giữ bước ở giới hạn tối đa
            direction = -1;        // Đổi hướng vào trong
        }
        else if (step <= 0)
        {                  // Nếu bước về 0
            step = 0;      // Đặt lại bước về 0
            direction = 1; // Đổi hướng ra ngoài
        }

        lastUpdate = now; // Cập nhật thời điểm cuối cùng
    }
}

void updateEffect7()
{                                        // Hàm cập nhật hiệu ứng nhấp nháy nửa vòng (Alternate half flashing)
    static bool state = false;           // Biến tĩnh lưu trạng thái nhấp nháy (true: nửa đầu, false: nửa sau)
    static unsigned long lastToggle = 0; // Biến tĩnh lưu thời điểm chuyển đổi trạng thái cuối cùng
    unsigned long now = millis();        // Lấy thời gian hiện tại (ms)

    if (now - lastToggle >= 300)
    {                     // Nếu đã qua 300ms kể từ lần chuyển đổi cuối cùng
        state = !state;   // Đảo trạng thái nhấp nháy
        lastToggle = now; // Cập nhật thời điểm chuyển đổi cuối cùng
        strip.show();     // Hiển thị màu lên dải LED
    }

    for (int i = 0; i < LED_COUNT; i++)
    { // Lặp qua từng LED
        if (i < LED_COUNT / 2)
        { // Nếu LED ở nửa đầu (0-5)
            if (state)
            { // Nếu trạng thái bật nửa đầu
                if (currentColor == 0)
                {                                               // Nếu ở chế độ cầu vồng
                    uint8_t pos = (i * 256 / LED_COUNT) & 0xFF; // Tính vị trí màu trên bánh xe
                    strip.setPixelColor(i, Wheel(pos));         // Đặt màu cầu vồng cho LED
                }
                else
                {                                         // Nếu ở chế độ màu cố định
                    strip.setPixelColor(i, currentColor); // Đặt màu cố định cho LED
                }
            }
            else
            {                              // Nếu trạng thái tắt nửa đầu
                strip.setPixelColor(i, 0); // Tắt LED
            }
        }
        else
        { // Nếu LED ở nửa sau (6-11)
            if (!state)
            { // Nếu trạng thái bật nửa sau
                if (currentColor == 0)
                {                                               // Nếu ở chế độ cầu vồng
                    uint8_t pos = (i * 256 / LED_COUNT) & 0xFF; // Tính vị trí màu trên bánh xe
                    strip.setPixelColor(i, Wheel(pos));         // Đặt màu cầu vồng cho LED
                }
                else
                {                                         // Nếu ở chế độ màu cố định
                    strip.setPixelColor(i, currentColor); // Đặt màu cố định cho LED
                }
            }
            else
            {                              // Nếu trạng thái tắt nửa sau
                strip.setPixelColor(i, 0); // Tắt LED
            }
        }
    }
}

void updateEffect8()
{                                               // Hàm cập nhật hiệu ứng di chuyển từ hai đầu (Moving from ends)
    static bool ledActive[LED_COUNT] = {false}; // Mảng tĩnh lưu trạng thái bật/tắt của từng LED
    static unsigned long lastRandomize = 0;     // Biến tĩnh lưu thời điểm chọn ngẫu nhiên LED cuối cùng
    static int activeLedCount = 0;              // Biến tĩnh lưu số lượng LED được bật
    static unsigned long lastShow = 0;          // Biến tĩnh lưu thời điểm hiển thị cuối cùng
    unsigned long now = millis();               // Lấy thời gian hiện tại (ms)

    if (now % 4000 == 2000 || activeLedCount == 0)
    {                                            // Nếu đến giữa chu kỳ 4000ms hoặc chưa có LED nào bật
        memset(ledActive, 0, sizeof(ledActive)); // Đặt lại tất cả LED về trạng thái tắt
        activeLedCount = random(6, 12);          // Chọn ngẫu nhiên số lượng LED từ 6 đến 11
        int selectedCount = 0;                   // Biến đếm số LED đã được chọn
        while (selectedCount < activeLedCount)
        {                                         // Lặp cho đến khi chọn đủ số LED
            int randomLed = random(0, LED_COUNT); // Chọn ngẫu nhiên một LED
            if (!ledActive[randomLed])
            {                                // Nếu LED chưa được bật
                ledActive[randomLed] = true; // Bật LED
                selectedCount++;             // Tăng số LED đã chọn
            }
        }
        lastRandomize = now; // Cập nhật thời điểm chọn ngẫu nhiên cuối cùng
    }

    float phase = (now % 4000) / 4000.0 * 2 * PI; // Tính pha của sóng cos trong chu kỳ 4000ms
    float factor = (cos(phase) + 1) / 2.0;        // Chuẩn hóa sóng cos từ -1..1 thành 0..1 để điều chỉnh độ sáng

    for (int i = 0; i < LED_COUNT; i++)
    {                                                   // Lặp qua từng LED
        uint8_t basePos = (i * 256 / LED_COUNT) & 0xFF; // Tính vị trí màu cơ bản trên bánh xe
        uint32_t baseColor = Wheel(basePos);            // Lấy màu cơ bản từ hàm Wheel

        if (ledActive[i])
        { // Nếu LED được bật
            if (currentColor == 0)
            {                                                    // Nếu ở chế độ cầu vồng
                uint8_t basePos = (i * 256 / LED_COUNT) & 0xFF;  // Tính lại vị trí màu (dư thừa)
                uint32_t baseColor = Wheel(basePos);             // Lấy lại màu cơ bản (dư thừa)
                uint8_t r = ((baseColor >> 16) & 0xFF) * factor; // Điều chỉnh đỏ theo factor
                uint8_t g = ((baseColor >> 8) & 0xFF) * factor;  // Điều chỉnh xanh lá theo factor
                uint8_t b = (baseColor & 0xFF) * factor;         // Điều chỉnh xanh dương theo factor
                strip.setPixelColor(i, strip.Color(r, g, b));    // Đặt màu đã điều chỉnh cho LED
            }
            else
            {                                                         // Nếu ở chế độ màu cố định
                uint8_t r = (((currentColor >> 16) & 0xFF)) * factor; // Điều chỉnh đỏ theo factor
                uint8_t g = (((currentColor >> 8) & 0xFF)) * factor;  // Điều chỉnh xanh lá theo factor
                uint8_t b = ((currentColor & 0xFF)) * factor;         // Điều chỉnh xanh dương theo factor
                strip.setPixelColor(i, strip.Color(r, g, b));         // Đặt màu đã điều chỉnh cho LED
            }
        }
        else
        {                              // Nếu LED không được bật
            strip.setPixelColor(i, 0); // Tắt LED
        }
    }

    if (now - lastShow >= 70)
    {                   // Nếu đã qua 70ms kể từ lần hiển thị cuối cùng
        strip.show();   // Hiển thị màu lên dải LED
        lastShow = now; // Cập nhật thời điểm hiển thị cuối cùng
    }
}

void updateEffect9()
{                                        // Hàm cập nhật hiệu ứng di chuyển đồng thời (Simultaneous movement)
    static float progress = 0.0f;        // Biến tĩnh lưu tiến trình di chuyển (0.0 - 1.0)
    static int direction = 1;            // Biến tĩnh lưu hướng di chuyển (1: tiến, -1: lùi)
    static unsigned long lastUpdate = 0; // Biến tĩnh lưu thời điểm cập nhật cuối cùng
    unsigned long now = millis();        // Lấy thời gian hiện tại (ms)

    if (now - lastUpdate >= 30)
    { // Nếu đã qua 30ms kể từ lần cập nhật cuối cùng
        for (int i = 0; i < LED_COUNT; i++)
        {                              // Lặp qua từng LED
            strip.setPixelColor(i, 0); // Tắt LED để chuẩn bị vẽ lại
        }

        float posA = 2.0f - (2.0f * progress); // Tính vị trí A di chuyển từ 2 về 0
        float posB = 3.0f + (2.0f * progress); // Tính vị trí B di chuyển từ 3 về 5
        float posC = 8.0f - (2.0f * progress); // Tính vị trí C di chuyển từ 8 về 6
        float posD = 9.0f + (2.0f * progress); // Tính vị trí D di chuyển từ 9 về 11

        for (int i = 0; i < LED_COUNT; i++)
        {                                                     // Lặp qua từng LED
            float intensity = 0.0f;                           // Biến lưu cường độ sáng tại vị trí i
            intensity = max(intensity, 1.0f - abs(i - posA)); // Tính cường độ dựa trên khoảng cách tới posA
            intensity = max(intensity, 1.0f - abs(i - posB)); // Tính cường độ dựa trên khoảng cách tới posB
            intensity = max(intensity, 1.0f - abs(i - posC)); // Tính cường độ dựa trên khoảng cách tới posC
            intensity = max(intensity, 1.0f - abs(i - posD)); // Tính cường độ dựa trên khoảng cách tới posD
            intensity = min(1.0f, intensity);                 // Giới hạn cường độ tối đa là 1.0

            if (intensity > 0.0f)
            { // Nếu cường độ lớn hơn 0 (LED sáng)
                if (currentColor == 0)
                {                                                       // Nếu ở chế độ cầu vồng
                    uint8_t basePos = (i * 256 / LED_COUNT) & 0xFF;     // Tính vị trí màu trên bánh xe
                    uint32_t baseColor = Wheel(basePos);                // Lấy màu cơ bản từ hàm Wheel
                    uint8_t r = ((baseColor >> 16) & 0xFF) * intensity; // Điều chỉnh đỏ theo cường độ
                    uint8_t g = ((baseColor >> 8) & 0xFF) * intensity;  // Điều chỉnh xanh lá theo cường độ
                    uint8_t b = (baseColor & 0xFF) * intensity;         // Điều chỉnh xanh dương theo cường độ
                    strip.setPixelColor(i, strip.Color(r, g, b));       // Đặt màu đã điều chỉnh cho LED
                }
                else
                {                                                            // Nếu ở chế độ màu cố định
                    uint8_t r = (((currentColor >> 16) & 0xFF)) * intensity; // Điều chỉnh đỏ theo cường độ
                    uint8_t g = (((currentColor >> 8) & 0xFF)) * intensity;  // Điều chỉnh xanh lá theo cường độ
                    uint8_t b = ((currentColor & 0xFF)) * intensity;         // Điều chỉnh xanh dương theo cường độ
                    strip.setPixelColor(i, strip.Color(r, g, b));            // Đặt màu đã điều chỉnh cho LED
                }
            }
        }

        progress += direction * 0.05f; // Cập nhật tiến trình di chuyển (tăng hoặc giảm 0.05)
        if (progress >= 1.0f)
        {                    // Nếu tiến trình đạt tối đa
            progress = 1.0f; // Giữ tiến trình ở 1.0
            direction = -1;  // Đổi hướng lùi
        }
        else if (progress <= 0.0f)
        {                    // Nếu tiến trình về 0
            progress = 0.0f; // Đặt lại tiến trình về 0
            direction = 1;   // Đổi hướng tiến
        }

        strip.show();     // Hiển thị màu lên dải LED
        lastUpdate = now; // Cập nhật thời điểm cuối cùng
    }
}

void updateEffect10()
{                                              // Hàm cập nhật hiệu ứng chuyển màu dần đều (Color transition)
    static unsigned long lastUpdate = 0;       // Biến tĩnh lưu thời điểm cập nhật cuối cùng
    static int fromIndex = 0;                  // Biến tĩnh lưu chỉ số màu bắt đầu trong colorSequence
    static int toIndex = 1;                    // Biến tĩnh lưu chỉ số màu đích trong colorSequence
    static float progress = 0.0;               // Biến tĩnh lưu tiến trình chuyển màu (0.0 - 1.0)
    const unsigned long transitionTime = 2000; // Định nghĩa thời gian chuyển màu là 2000ms
    const unsigned long updateInterval = 30;   // Định nghĩa khoảng thời gian cập nhật là 30ms

    unsigned long now = millis(); // Lấy thời gian hiện tại (ms)
    if (now - lastUpdate >= updateInterval)
    {                                                             // Nếu đã qua 30ms kể từ lần cập nhật cuối cùng
        float delta = (now - lastUpdate) / (float)transitionTime; // Tính tiến trình tăng thêm dựa trên thời gian
        progress += delta;                                        // Tăng tiến trình chuyển màu
        if (progress >= 1.0)
        {                                        // Nếu tiến trình đạt tối đa
            progress = 0.0;                      // Đặt lại tiến trình về 0
            fromIndex = toIndex;                 // Chuyển màu bắt đầu thành màu đích
            toIndex = (toIndex + 1) % numColors; // Tăng chỉ số màu đích, quay lại 0 nếu vượt quá numColors
        }

        uint32_t newColor = blendColors(colorSequence[fromIndex], colorSequence[toIndex], progress); // Trộn hai màu theo tiến trình

        setAllLEDs(newColor); // Đặt tất cả LED thành màu mới
        strip.show();         // Hiển thị màu lên dải LED
        lastUpdate = now;     // Cập nhật thời điểm cuối cùng
    }
}

uint32_t blendColors(uint32_t baseColor, uint32_t targetColor, float ratio)
{                                          // Hàm trộn hai màu theo tỷ lệ
    uint8_t r1 = (baseColor >> 16) & 0xFF; // Tách thành phần đỏ của màu cơ bản
    uint8_t g1 = (baseColor >> 8) & 0xFF;  // Tách thành phần xanh lá của màu cơ bản
    uint8_t b1 = baseColor & 0xFF;         // Tách thành phần xanh dương của màu cơ bản

    uint8_t r2 = (targetColor >> 16) & 0xFF; // Tách thành phần đỏ của màu đích
    uint8_t g2 = (targetColor >> 8) & 0xFF;  // Tách thành phần xanh lá của màu đích
    uint8_t b2 = targetColor & 0xFF;         // Tách thành phần xanh dương của màu đích

    uint8_t r = (uint8_t)(r1 * (1 - ratio) + r2 * ratio); // Trộn đỏ theo tỷ lệ
    uint8_t g = (uint8_t)(g1 * (1 - ratio) + g2 * ratio); // Trộn xanh lá theo tỷ lệ
    uint8_t b = (uint8_t)(b1 * (1 - ratio) + b2 * ratio); // Trộn xanh dương theo tỷ lệ

    return strip.Color(r, g, b); // Trả về màu đã trộn
}

void updateAnimation()
{ // Hàm cập nhật animation dựa trên hiệu ứng hiện tại
    if (!ledOn)
        return; // Nếu LED tắt, thoát hàm
    if (animationMode && animationPlaying)
    { // Nếu ở chế độ animation và đang phát
        switch (currentEffect)
        { // Chọn hiệu ứng để cập nhật dựa trên currentEffect
        case 0:
            updateEffect0();
            break; // Gọi hàm hiệu ứng cầu vồng quay
        case 1:
            updateEffect1();
            break; // Gọi hàm hiệu ứng thở
        case 2:
            updateEffect2();
            break; // Gọi hàm hiệu ứng nhấp nháy xen kẽ
        case 3:
            updateEffect3();
            break; // Gọi hàm hiệu ứng đuổi theo
        case 4:
            updateEffect4();
            break; // Gọi hàm hiệu ứng sóng
        case 5:
            updateEffect5();
            break; // Gọi hàm hiệu ứng lấp lánh
        case 6:
            updateEffect6();
            break; // Gọi hàm hiệu ứng pháo hoa
        case 7:
            updateEffect7();
            break; // Gọi hàm hiệu ứng nhấp nháy nửa vòng
        case 8:
            updateEffect8();
            break; // Gọi hàm hiệu ứng di chuyển từ hai đầu
        case 9:
            updateEffect9();
            break; // Gọi hàm hiệu ứng di chuyển đồng thời
        case 10:
            updateEffect10();
            break; // Gọi hàm hiệu ứng chuyển màu dần đều
        default:
            break; // Nếu không có hiệu ứng nào, không làm gì
        }
    }
}

void changeColor()
{                                                            // Hàm chuyển sang màu tiếp theo trong chuỗi
    currentColorIndex = (currentColorIndex + 1) % numColors; // Tăng chỉ số màu, quay lại 0 nếu vượt quá numColors
    if (currentColorIndex == 0)
    {                     // Nếu chỉ số là 0 (chế độ cầu vồng)
        currentColor = 0; // Đặt màu hiện tại là 0 (cầu vồng)
    }
    else
    {                                                    // Nếu chỉ số khác 0
        currentColor = colorSequence[currentColorIndex]; // Lấy màu từ mảng colorSequence
    }
}

void reverseColor()
{                                                            // Hàm chuyển sang màu trước đó trong chuỗi
    currentColorIndex = (currentColorIndex - 1) % numColors; // Giảm chỉ số màu, quay lại numColors-1 nếu nhỏ hơn 0
    if (currentColorIndex == 0)
    {                     // Nếu chỉ số là 0 (chế độ cầu vồng)
        currentColor = 0; // Đặt màu hiện tại là 0 (cầu vồng)
    }
    else
    {                                                    // Nếu chỉ số khác 0
        currentColor = colorSequence[currentColorIndex]; // Lấy màu từ mảng colorSequence
    }
}

void updateTestEffect()
{                                 // Hàm cập nhật hiệu ứng test (nhấp nháy 4 màu)
    unsigned long now = millis(); // Lấy thời gian hiện tại (ms)
    if (now - testEffectStart < 2000)
    { // Nếu chưa đủ 2000ms kể từ khi bắt đầu
        if (now - lastTestUpdate >= 200)
        {                                                // Nếu đã qua 200ms kể từ lần cập nhật cuối cùng
            lastTestUpdate = now;                        // Cập nhật thời điểm cuối cùng
            testEffectIndex = (testEffectIndex + 1) % 4; // Tăng chỉ số màu, quay lại 0 nếu vượt quá 3
            setAllLEDs(testColors[testEffectIndex]);     // Đặt tất cả LED thành màu mới từ mảng testColors
        }
    }
    else
    {                             // Nếu đã qua 2000ms
        testEffectActive = false; // Tắt hiệu ứng test
        updateStaticDisplay();    // Cập nhật hiển thị tĩnh
    }
}

void handleIR()
{ // Hàm xử lý tín hiệu hồng ngoại từ remote
    if (testEffectActive)
        return; // Nếu hiệu ứng test đang chạy, thoát hàm

    static unsigned long lastIRTime = 0; // Biến tĩnh lưu thời điểm nhận tín hiệu IR cuối cùng
    unsigned long now = millis();        // Lấy thời gian hiện tại (ms)

    if (now - lastIRTime < 150)
    {           // Nếu chưa qua 150ms kể từ tín hiệu cuối cùng (debounce)
        return; // Thoát hàm để tránh xử lý lặp lại
    }

    if (IrReceiver.decode())
    { // Nếu nhận được tín hiệu IR hợp lệ
        if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)
        {                        // Nếu tín hiệu là lặp lại
            IrReceiver.resume(); // Tiếp tục nhận tín hiệu IR
            return;              // Thoát hàm
        }

        uint32_t code = IrReceiver.decodedIRData.decodedRawData;              // Lấy mã tín hiệu IR đã giải mã
        Serial.print("Raw Data (HEX): 0x");                                   // In nhãn dữ liệu thô ra Serial
        Serial.println(code, HEX);                                            // In mã tín hiệu dưới dạng HEX
        Serial.print("Protocol: ");                                           // In nhãn giao thức
        Serial.println(getProtocolString(IrReceiver.decodedIRData.protocol)); // In tên giao thức IR

        switch (code)
        {                // Xử lý mã tín hiệu IR
        case 0xB847FF00: // Nếu nhận nút Menu: chuyển đổi chế độ
            if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)
            {                        // Nếu là tín hiệu lặp lại
                IrReceiver.resume(); // Tiếp tục nhận tín hiệu IR
                break;               // Thoát switch case
            }
            mode = !mode; // Đảo chế độ (true: code 1, false: code 2)
            if (mode)
            {                            // Nếu chuyển sang chế độ code 1
                animationMode = true;    // Bật chế độ animation
                animationPlaying = true; // Phát animation
                updateStaticDisplay();   // Cập nhật hiển thị tĩnh ban đầu
            }
            else
            {                        // Nếu chuyển sang chế độ code 2
                gradient = 0;        // Đặt lại gradient về 0
                avgVol = 0;          // Đặt lại trung bình âm lượng về 0
                maxVol = 1;          // Đặt lại âm lượng tối đa về 1
                turnOffLEDs();       // Tắt tất cả LED
                delay(200);          // Chờ 200ms để debounce
                IrReceiver.resume(); // Tiếp tục nhận tín hiệu IR
            }
            break;
        case 0xBA45FF00:    // Nếu nhận nút Power: bật/tắt đèn
            ledOn = !ledOn; // Đảo trạng thái LED
            if (ledOn)
            {                                // Nếu bật LED
                if (animationMode)           // Nếu ở chế độ animation
                    animationPlaying = true; // Tiếp tục phát animation
                else                         // Nếu không ở chế độ animation
                    updateStaticDisplay();   // Cập nhật hiển thị tĩnh
            }
            else
            {                  // Nếu tắt LED
                turnOffLEDs(); // Tắt tất cả LED
            }
            break;
        case 0xBB44FF00:                // Nếu nhận nút Test: nhấp nháy 4 màu
            testEffectActive = true;    // Kích hoạt hiệu ứng test
            testEffectStart = millis(); // Lưu thời điểm bắt đầu
            lastTestUpdate = millis();  // Lưu thời điểm cập nhật đầu tiên
            testEffectIndex = 0;        // Đặt chỉ số màu về 0
            break;
        case 0xBF40FF00: // Nếu nhận nút Plus: tăng độ sáng
            if (brightness < 255)
            {                                           // Nếu độ sáng chưa đạt tối đa
                brightness = min(brightness + 25, 255); // Tăng độ sáng thêm 25, tối đa 255
                strip.setBrightness(brightness);        // Cập nhật độ sáng cho dải LED
                if (!animationMode)                     // Nếu không ở chế độ animation
                    updateStaticDisplay();              // Cập nhật hiển thị tĩnh
            }
            break;
        case 0xE619FF00: // Nếu nhận nút Minus: giảm độ sáng
            if (brightness > 0)
            {                                         // Nếu độ sáng lớn hơn 0
                brightness = max(brightness - 25, 0); // Giảm độ sáng 25, tối thiểu 0
                strip.setBrightness(brightness);      // Cập nhật độ sáng cho dải LED
                if (!animationMode)                   // Nếu không ở chế độ animation
                    updateStaticDisplay();            // Cập nhật hiển thị tĩnh
            }
            break;
        case 0xF609FF00:   // Nếu nhận nút Right: chuyển màu tiếp theo
            changeColor(); // Gọi hàm chuyển sang màu tiếp theo
            break;
        case 0xF807FF00:    // Nếu nhận nút Left: chuyển màu trước đó
            reverseColor(); // Gọi hàm chuyển sang màu trước đó
            break;
        case 0xEA15FF00: // Nếu nhận nút Play: tạm dừng/tiếp tục animation
            if (animationMode)
            {                                                            // Nếu ở chế độ animation
                animationPlaying = !animationPlaying;                    // Đảo trạng thái phát animation
                Serial.print("Animation playing: ");                     // In nhãn trạng thái
                Serial.println(animationPlaying ? "Resumed" : "Paused"); // In trạng thái hiện tại
            }
            break;
        case 0xBC43FF00:                               // Nếu nhận nút Back: đặt LED thành màu trắng
            animationMode = false;                     // Tắt chế độ animation
            animationPlaying = false;                  // Dừng phát animation
            currentColor = strip.Color(255, 255, 255); // Đặt màu hiện tại là trắng
            currentColorIndex = 1;                     // Đặt chỉ số màu là 1 (không chính xác với mảng, nên là tạm thời)
            setAllLEDs(currentColor);                  // Đặt tất cả LED thành màu trắng
            break;
        case 0xE916FF00:                    // Nếu nhận nút 0: kích hoạt hiệu ứng Rainbow Cycle
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 0;              // Đặt hiệu ứng hiện tại là 0
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        case 0xF30CFF00:                    // Nếu nhận nút 1: kích hoạt hiệu ứng Breathing
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 1;              // Đặt hiệu ứng hiện tại là 1
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        case 0xE718FF00:                    // Nếu nhận nút 2: kích hoạt hiệu ứng Alternating flashing
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 2;              // Đặt hiệu ứng hiện tại là 2
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        case 0xA15EFF00:                    // Nếu nhận nút 3: kích hoạt hiệu ứng Moving trail
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 3;              // Đặt hiệu ứng hiện tại là 3
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        case 0xF708FF00:                    // Nếu nhận nút 4: kích hoạt hiệu ứng Wave
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 4;              // Đặt hiệu ứng hiện tại là 4
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        case 0xE31CFF00:                    // Nếu nhận nút 5: kích hoạt hiệu ứng Sparkle
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 5;              // Đặt hiệu ứng hiện tại là 5
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        case 0xA55AFF00:                    // Nếu nhận nút 6: kích hoạt hiệu ứng Fireworks
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 6;              // Đặt hiệu ứng hiện tại là 6
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        case 0xBD42FF00:                    // Nếu nhận nút 7: kích hoạt hiệu ứng Alternate half flashing
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 7;              // Đặt hiệu ứng hiện tại là 7
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        case 0xAD52FF00:                    // Nếu nhận nút 8: kích hoạt hiệu ứng Moving from ends
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 8;              // Đặt hiệu ứng hiện tại là 8
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        case 0xB54AFF00:                    // Nếu nhận nút 9: kích hoạt hiệu ứng Simultaneous movement
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 9;              // Đặt hiệu ứng hiện tại là 9
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        case 0xF20DFF00:                    // Nếu nhận nút C: kích hoạt hiệu ứng Color transition
            previousEffect = currentEffect; // Lưu hiệu ứng hiện tại
            currentEffect = 10;             // Đặt hiệu ứng hiện tại là 10
            animationMode = true;           // Bật chế độ animation
            animationPlaying = true;        // Phát animation
            break;
        default:   // Nếu mã không khớp
            break; // Không làm gì
        }

        Serial.println("-------------------"); // In dấu phân cách ra Serial
        lastIRTime = now;                      // Cập nhật thời điểm nhận tín hiệu IR cuối cùng
        IrReceiver.resume();                   // Tiếp tục nhận tín hiệu IR
    }
}

void Visualize()
{ // Hàm thực hiện hiệu ứng visual dựa trên âm thanh
    if (IrReceiver.decode())
    {                                                            // Nếu nhận được tín hiệu IR trong khi chạy code 2
        uint32_t code = IrReceiver.decodedIRData.decodedRawData; // Lấy mã tín hiệu IR
        if (code == 0xB847FF00)
        {                        // Nếu nhận nút Menu (0xB847FF00)
            mode = true;         // Chuyển sang chế độ code 1 (IR)
            IrReceiver.resume(); // Tiếp tục nhận tín hiệu IR
            return;              // Thoát hàm ngay lập tức
        }
        IrReceiver.resume(); // Tiếp tục nhận tín hiệu IR nếu không phải nút Menu
    }
    switch (visual)
    { // Chọn hiệu ứng visual dựa trên biến visual
    case 0:
        VU();
        break; // Gọi hàm hiệu ứng VU cơ bản
    case 1:
        VUdot();
        break; // Gọi hàm hiệu ứng VU với chấm đỉnh
    case 2:
        VU1();
        break; // Gọi hàm hiệu ứng VU đối xứng từ giữa
    case 3:
        VU2();
        break; // Gọi hàm hiệu ứng VU đối xứng từ hai đầu
    case 4:
        Pulse();
        break; // Gọi hàm hiệu ứng Pulse
    case 5:
        Traffic();
        break; // Gọi hàm hiệu ứng Traffic
    default:
        PaletteDance();
        break; // Mặc định: gọi hàm hiệu ứng PaletteDance
    }
}

void Pulse()
{               // Hàm thực hiện hiệu ứng Pulse dựa trên âm lượng
    fade(0.75); // Làm mờ toàn bộ LED với hệ số 0.75 (75% độ sáng cũ)
    if (bump)
        gradient += thresholds[palette] / 24; // Nếu có "bump", tăng gradient theo ngưỡng palette
    if (volume > 0)
    {                                                                                   // Nếu âm lượng lớn hơn 0
        uint32_t col = ColorPalette(-1);                                                // Lấy màu hiện tại từ palette (dùng gradient)
        int start = LED_HALF - (LED_HALF * (volume / maxVol));                          // Tính vị trí bắt đầu dựa trên âm lượng
        int finish = LED_HALF + (LED_HALF * (volume / maxVol)) + strip.numPixels() % 2; // Tính vị trí kết thúc
        for (int i = start; i < finish; i++)
        {                                                               // Lặp từ start đến finish
            float damp = sin((i - start) * PI / float(finish - start)); // Tính hệ số giảm dần theo sóng sin
            damp = pow(damp, 2.0);                                      // Bình phương hệ số để tạo hiệu ứng mượt hơn
            uint32_t col2 = strip.getPixelColor(-1);                    // Lấy màu hiện tại của LED (sai logic, nên là i)
            uint8_t colors[3];                                          // Mảng lưu giá trị RGB mới
            float avgCol = 0, avgCol2 = 0;                              // Biến lưu trung bình màu mới và cũ
            for (int k = 0; k < 3; k++)
            {                                                                      // Lặp qua 3 thành phần RGB
                colors[k] = split(col, k) * damp * knob * pow(volume / maxVol, 2); // Tính giá trị RGB mới
                avgCol += colors[k];                                               // Cộng dồn để tính trung bình màu mới
                avgCol2 += split(col2, k);                                         // Cộng dồn để tính trung bình màu cũ
            }
            avgCol /= 3.0, avgCol2 /= 3.0; // Tính trung bình màu
            if (avgCol > avgCol2)
                strip.setPixelColor(i, strip.Color(colors[0], colors[1], colors[2])); // Nếu màu mới sáng hơn, cập nhật LED
        }
    }
    strip.show(); // Hiển thị màu lên dải LED
}

void VU()
{                                         // Hàm thực hiện hiệu ứng VU cơ bản
    unsigned long startMillis = millis(); // Lấy thời gian bắt đầu lấy mẫu (ms)
    float peakToPeak = 0;                 // Biến lưu biên độ đỉnh-đỉnh của tín hiệu âm thanh
    unsigned int signalMax = 0;           // Biến lưu giá trị tối đa của tín hiệu
    unsigned int signalMin = 1023;        // Biến lưu giá trị tối thiểu của tín hiệu
    unsigned int c, y;                    // Biến lưu số LED sáng và vị trí đỉnh
    while (millis() - startMillis < SAMPLE_WINDOW)
    {                                       // Lấy mẫu trong 10ms
        sample = analogRead(AUDIO_PIN) * 2; // Đọc tín hiệu âm thanh từ A0 và nhân đôi
        if (sample < 1024)
        { // Nếu mẫu hợp lệ (nhỏ hơn 1024)
            if (sample > signalMax)
                signalMax = sample; // Cập nhật giá trị tối đa
            else if (sample < signalMin)
                signalMin = sample; // Cập nhật giá trị tối thiểu
        }
    }
    peakToPeak = signalMax - signalMin; // Tính biên độ đỉnh-đỉnh
    for (int i = 0; i <= strip.numPixels() - 1; i++)
    {                                                                             // Lặp qua tất cả LED
        strip.setPixelColor(i, Wheel(map(i, 0, strip.numPixels() - 1, 10, 200))); // Đặt màu cầu vồng cho LED
    }
    c = fscale(INPUT_FLOOR, INPUT_CEILING, strip.numPixels(), 0, peakToPeak, 2); // Chuyển đổi biên độ thành số LED sáng
    if (c <= strip.numPixels())
    {                                                                             // Nếu số LED sáng hợp lệ
        drawLine(strip.numPixels(), strip.numPixels() - c, strip.Color(0, 0, 0)); // Tắt các LED từ cuối đến c
    }
    y = strip.numPixels() - peak;                                                 // Tính vị trí đỉnh hiện tại
    strip.setPixelColor(y - 1, Wheel(map(y, 0, strip.numPixels() - 1, 10, 200))); // Đặt màu cho vị trí đỉnh
    strip.show();                                                                 // Hiển thị màu lên dải LED
    if (dotHangCount > PEAK_HANG)
    { // Nếu thời gian giữ đỉnh vượt quá PEAK_HANG
        if (++dotCount >= PEAK_FALL)
        {                 // Nếu đếm đủ chu kỳ giảm đỉnh
            peak++;       // Tăng vị trí đỉnh (giảm độ sáng)
            dotCount = 0; // Đặt lại bộ đếm
        }
    }
    else
    {                   // Nếu chưa vượt quá thời gian giữ đỉnh
        dotHangCount++; // Tăng thời gian giữ đỉnh
    }
}

void VUdot()
{                                         // Hàm thực hiện hiệu ứng VU với chấm đỉnh
    unsigned long startMillis = millis(); // Lấy thời gian bắt đầu lấy mẫu (ms)
    float peakToPeak = 0;                 // Biến lưu biên độ đỉnh-đỉnh của tín hiệu âm thanh
    unsigned int signalMax = 0;           // Biến lưu giá trị tối đa của tín hiệu
    unsigned int signalMin = 1023;        // Biến lưu giá trị tối thiểu của tín hiệu
    unsigned int c, y;                    // Biến lưu số LED sáng và vị trí đỉnh
    while (millis() - startMillis < SAMPLE_WINDOW)
    {                                       // Lấy mẫu trong 10ms
        sample = analogRead(AUDIO_PIN) * 3; // Đọc tín hiệu âm thanh từ A0 và nhân ba
        if (sample < 1024)
        { // Nếu mẫu hợp lệ (nhỏ hơn 1024)
            if (sample > signalMax)
                signalMax = sample; // Cập nhật giá trị tối đa
            else if (sample < signalMin)
                signalMin = sample; // Cập nhật giá trị tối thiểu
        }
    }
    peakToPeak = signalMax - signalMin; // Tính biên độ đỉnh-đỉnh
    for (int i = 0; i <= strip.numPixels() - 1; i++)
    {                                                                             // Lặp qua tất cả LED
        strip.setPixelColor(i, Wheel(map(i, 0, strip.numPixels() - 1, 10, 200))); // Đặt màu cầu vồng cho LED
    }
    c = fscale(INPUT_FLOOR, INPUT_CEILING, strip.numPixels(), 0, peakToPeak, 2); // Chuyển đổi biên độ thành số LED sáng
    if (c < peak)
    {                     // Nếu số LED sáng nhỏ hơn đỉnh hiện tại
        peak = c;         // Cập nhật đỉnh mới
        dotHangCount = 0; // Đặt lại thời gian giữ đỉnh
    }
    if (c <= strip.numPixels())
    {                                                                             // Nếu số LED sáng hợp lệ
        drawLine(strip.numPixels(), strip.numPixels() - c, strip.Color(0, 0, 0)); // Tắt các LED từ cuối đến c
    }
    y = strip.numPixels() - peak;                                                 // Tính vị trí đỉnh hiện tại
    strip.setPixelColor(y - 1, Wheel(map(y, 0, strip.numPixels() - 1, 10, 200))); // Đặt màu cho vị trí đỉnh
    strip.show();                                                                 // Hiển thị màu lên dải LED
    if (dotHangCount > PEAK_HANG)
    { // Nếu thời gian giữ đỉnh vượt quá PEAK_HANG
        if (++dotCount >= PEAK_FALL)
        {                 // Nếu đếm đủ chu kỳ giảm đỉnh
            peak++;       // Tăng vị trí đỉnh (giảm độ sáng)
            dotCount = 0; // Đặt lại bộ đếm
        }
    }
    else
    {                   // Nếu chưa vượt quá thời gian giữ đỉnh
        dotHangCount++; // Tăng thời gian giữ đỉnh
    }
}

void VU1()
{                                         // Hàm thực hiện hiệu ứng VU đối xứng từ giữa
    unsigned long startMillis = millis(); // Lấy thời gian bắt đầu lấy mẫu (ms)
    float peakToPeak = 0;                 // Biến lưu biên độ đỉnh-đỉnh của tín hiệu âm thanh
    unsigned int signalMax = 0;           // Biến lưu giá trị tối đa của tín hiệu
    unsigned int signalMin = 1023;        // Biến lưu giá trị tối thiểu của tín hiệu
    unsigned int c, y;                    // Biến lưu số LED sáng và vị trí đỉnh
    while (millis() - startMillis < SAMPLE_WINDOW)
    {                                       // Lấy mẫu trong 10ms
        sample = analogRead(AUDIO_PIN) * 3; // Đọc tín hiệu âm thanh từ A0 và nhân ba
        Serial.println(sample);             // In giá trị mẫu ra Serial để debug
        if (sample < 1024)
        { // Nếu mẫu hợp lệ (nhỏ hơn 1024)
            if (sample > signalMax)
                signalMax = sample; // Cập nhật giá trị tối đa
            else if (sample < signalMin)
                signalMin = sample; // Cập nhật giá trị tối thiểu
        }
    }
    peakToPeak = signalMax - signalMin; // Tính biên độ đỉnh-đỉnh
    for (int i = 0; i <= LED_HALF - 1; i++)
    {                                                   // Lặp qua nửa dải LED
        uint32_t color = Wheel(map(i, 0, -1, 30, 150)); // Tính màu cầu vồng (lỗi logic map)
        strip.setPixelColor(LED_TOTAL - i, color);      // Đặt màu cho LED đối xứng từ cuối
        strip.setPixelColor(0 + i, color);              // Đặt màu cho LED từ đầu
    }
    c = fscale(INPUT_FLOOR, INPUT_CEILING, LED_HALF, 0, peakToPeak, 2); // Chuyển đổi biên độ thành số LED sáng nửa dải
    if (c < peak)
    {                     // Nếu số LED sáng nhỏ hơn đỉnh hiện tại
        peak = c;         // Cập nhật đỉnh mới
        dotHangCount = 0; // Đặt lại thời gian giữ đỉnh
    }
    if (c <= strip.numPixels())
    {                                                           // Nếu số LED sáng hợp lệ
        drawLine(LED_HALF, LED_HALF - c, strip.Color(0, 0, 0)); // Tắt LED từ giữa ra trái
        drawLine(LED_HALF, LED_HALF + c, strip.Color(0, 0, 0)); // Tắt LED từ giữa ra phải
    }
    y = LED_HALF - peak;                                       // Tính vị trí đỉnh bên trái
    uint32_t color1 = Wheel(map(y, 0, LED_HALF - 1, 30, 150)); // Tính màu cho đỉnh
    strip.setPixelColor(y - 1, color1);                        // Đặt màu cho đỉnh bên trái
    y = LED_HALF + peak;                                       // Tính vị trí đỉnh bên phải
    strip.setPixelColor(y, color1);                            // Đặt màu cho đỉnh bên phải
    strip.show();                                              // Hiển thị màu lên dải LED
    if (dotHangCount > PEAK_HANG)
    { // Nếu thời gian giữ đỉnh vượt quá PEAK_HANG
        if (++dotCount >= PEAK_FALL)
        {                 // Nếu đếm đủ chu kỳ giảm đỉnh
            peak++;       // Tăng vị trí đỉnh (giảm độ sáng)
            dotCount = 0; // Đặt lại bộ đếm
        }
    }
    else
    {                   // Nếu chưa vượt quá thời gian giữ đỉnh
        dotHangCount++; // Tăng thời gian giữ đỉnh
    }
}

void VU2()
{                                         // Hàm thực hiện hiệu ứng VU đối xứng từ hai đầu
    unsigned long startMillis = millis(); // Lấy thời gian bắt đầu lấy mẫu (ms)
    float peakToPeak = 0;                 // Biến lưu biên độ đỉnh-đỉnh của tín hiệu âm thanh
    unsigned int signalMax = 0;           // Biến lưu giá trị tối đa của tín hiệu
    unsigned int signalMin = 1023;        // Biến lưu giá trị tối thiểu của tín hiệu
    unsigned int c, y;                    // Biến lưu số LED sáng và vị trí đỉnh
    while (millis() - startMillis < SAMPLE_WINDOW)
    {                                       // Lấy mẫu trong 10ms
        sample = analogRead(AUDIO_PIN) * 3; // Đọc tín hiệu âm thanh từ A0 và nhân ba
        Serial.println(sample);             // In giá trị mẫu ra Serial để debug
        if (sample < 1024)
        { // Nếu mẫu hợp lệ (nhỏ hơn 1024)
            if (sample > signalMax)
                signalMax = sample; // Cập nhật giá trị tối đa
            else if (sample < signalMin)
                signalMin = sample; // Cập nhật giá trị tối thiểu
        }
    }
    peakToPeak = signalMax - signalMin; // Tính biên độ đỉnh-đỉnh
    for (int i = 0; i <= LED_HALF - 1; i++)
    {                                                             // Lặp qua nửa dải LED
        uint32_t color = Wheel(map(i, 0, LED_HALF - 1, 30, 150)); // Tính màu cầu vồng cho LED
        strip.setPixelColor(LED_HALF - i - 1, color);             // Đặt màu cho LED từ giữa ra trái
        strip.setPixelColor(LED_HALF + i, color);                 // Đặt màu cho LED từ giữa ra phải
    }
    c = fscale(INPUT_FLOOR, INPUT_CEILING, LED_HALF, 0, peakToPeak, 2); // Chuyển đổi biên độ thành số LED sáng nửa dải
    if (c < peak)
    {                     // Nếu số LED sáng nhỏ hơn đỉnh hiện tại
        peak = c;         // Cập nhật đỉnh mới
        dotHangCount = 0; // Đặt lại thời gian giữ đỉnh
    }
    if (c <= LED_TOTAL)
    {                                                             // Nếu số LED sáng hợp lệ
        drawLine(LED_TOTAL, LED_TOTAL - c, strip.Color(0, 0, 0)); // Tắt LED từ cuối vào
        drawLine(0, 0 + c, strip.Color(0, 0, 0));                 // Tắt LED từ đầu vào
    }
    y = LED_TOTAL - peak;                                                           // Tính vị trí đỉnh từ cuối
    strip.setPixelColor(y - 1, Wheel(map(LED_HALF + y, 0, LED_HALF - 1, 30, 150))); // Đặt màu cho đỉnh từ cuối (lỗi logic map)
    y = 0 + peak;                                                                   // Tính vị trí đỉnh từ đầu
    strip.setPixelColor(y, Wheel(map(LED_HALF - y, 0, LED_HALF + 1, 30, 150)));     // Đặt màu cho đỉnh từ đầu (lỗi logic map)
    strip.show();                                                                   // Hiển thị màu lên dải LED
    if (dotHangCount > PEAK_HANG)
    { // Nếu thời gian giữ đỉnh vượt quá PEAK_HANG
        if (++dotCount >= PEAK_FALL)
        {                 // Nếu đếm đủ chu kỳ giảm đỉnh
            peak++;       // Tăng vị trí đỉnh (giảm độ sáng)
            dotCount = 0; // Đặt lại bộ đếm
        }
    }
    else
    {                   // Nếu chưa vượt quá thời gian giữ đỉnh
        dotHangCount++; // Tăng thời gian giữ đỉnh
    }
}

void Traffic()
{              // Hàm thực hiện hiệu ứng Traffic (chấm di chuyển)
    fade(0.8); // Làm mờ toàn bộ LED với hệ số 0.8 (80% độ sáng cũ)
    if (bump)
    {                    // Nếu có "bump" (tăng đột ngột âm lượng)
        int8_t slot = 0; // Biến lưu vị trí trống để thêm chấm mới
        for (slot; slot < sizeof(pos); slot++)
        { // Lặp qua mảng pos
            if (pos[slot] < -1)
                break; // Nếu tìm thấy vị trí trống (-2), thoát vòng lặp
            else if (slot + 1 >= sizeof(pos))
            {              // Nếu không còn chỗ trống
                slot = -3; // Đặt slot thành -3 để báo hiệu không thêm được
                break;
            }
        }
        if (slot != -3)
        {                                                         // Nếu tìm thấy vị trí trống
            pos[slot] = (slot % 2 == 0) ? -1 : strip.numPixels(); // Đặt chấm bắt đầu từ trái (-1) hoặc phải (12)
            uint32_t col = ColorPalette(-1);                      // Lấy màu hiện tại từ palette
            gradient += thresholds[palette] / 24;                 // Tăng gradient theo ngưỡng palette
            for (int j = 0; j < 3; j++)
            {                                 // Lặp qua 3 thành phần RGB
                rgb[slot][j] = split(col, j); // Lưu giá trị RGB cho chấm mới
            }
        }
    }
    if (volume > 0)
    { // Nếu âm lượng lớn hơn 0
        for (int i = 0; i < sizeof(pos); i++)
        { // Lặp qua mảng pos
            if (pos[i] < -1)
                continue;               // Nếu vị trí không hợp lệ (-2), bỏ qua
            pos[i] += (i % 2) ? -1 : 1; // Di chuyển chấm: chẵn thì sang phải, lẻ thì sang trái
            if (pos[i] >= strip.numPixels())
                pos[i] = -2;                                                                     // Nếu chấm ra ngoài dải, đánh dấu không hợp lệ
            strip.setPixelColor(pos[i], strip.Color(                                             // Đặt màu cho chấm dựa trên âm lượng
                                            float(rgb[i][0]) * pow(volume / maxVol, 2.0) * knob, // Điều chỉnh đỏ
                                            float(rgb[i][1]) * pow(volume / maxVol, 2.0) * knob, // Điều chỉnh xanh lá
                                            float(rgb[i][2]) * pow(volume / maxVol, 2.0) * knob) // Điều chỉnh xanh dương
            );
        }
    }
    strip.show(); // Hiển thị màu lên dải LED
}

void PaletteDance()
{ // Hàm thực hiện hiệu ứng PaletteDance (chấm nhảy theo âm thanh)
    if (bump)
        left = !left; // Nếu có "bump", đổi hướng di chuyển của chấm
    if (volume > avgVol)
    { // Nếu âm lượng lớn hơn trung bình
        for (int i = 0; i < strip.numPixels(); i++)
        {                                                             // Lặp qua tất cả LED
            float sinVal = abs(sin(                                   // Tính giá trị sóng sin để tạo hiệu ứng nhấp nháy
                (i + dotPos) * (PI / float(strip.numPixels() / 1.25)) // Dựa trên vị trí LED và chấm
                ));
            sinVal *= sinVal;                                   // Bình phương để tăng độ mượt
            sinVal *= volume / maxVol;                          // Điều chỉnh theo âm lượng
            sinVal *= knob;                                     // Điều chỉnh theo núm
            unsigned int val = float(thresholds[palette] + 1) * // Tính giá trị màu trong palette
                                   (float(i + map(dotPos, -1 * (strip.numPixels() - 1), strip.numPixels() - 1, 0, strip.numPixels() - 1)) /
                                    float(strip.numPixels())) +
                               (gradient);
            val %= thresholds[palette];                               // Giới hạn giá trị trong ngưỡng palette
            uint32_t col = ColorPalette(val);                         // Lấy màu từ palette
            strip.setPixelColor(i, strip.Color(                       // Đặt màu cho LED
                                       float(split(col, 0)) * sinVal, // Điều chỉnh đỏ
                                       float(split(col, 1)) * sinVal, // Điều chỉnh xanh lá
                                       float(split(col, 2)) * sinVal) // Điều chỉnh xanh dương
            );
        }
        dotPos += (left) ? -1 : 1; // Di chuyển chấm: trái (-1) hoặc phải (+1)
    }
    else
        fade(0.8); // Nếu âm lượng nhỏ hơn trung bình, làm mờ LED
    strip.show();  // Hiển thị màu lên dải LED
    if (dotPos < 0)
        dotPos = strip.numPixels() - strip.numPixels() / 6; // Nếu chấm ra ngoài trái, đặt lại gần cuối
    else if (dotPos >= strip.numPixels() - strip.numPixels() / 6)
        dotPos = 0; // Nếu chấm ra ngoài phải, đặt lại đầu
}

void CyclePalette()
{ // Hàm chuyển đổi palette tự động
    if (shuffle && millis() / 1000.0 - shuffleTime > 38 && gradient % 2)
    {                                    // Nếu bật shuffle và qua 38 giây
        shuffleTime = millis() / 1000.0; // Cập nhật thời điểm chuyển đổi
        palette++;                       // Tăng chỉ số palette
        if (palette >= sizeof(thresholds) / 2)
            palette = 0;                 // Nếu vượt quá số palette, quay lại 0
        gradient %= thresholds[palette]; // Điều chỉnh gradient theo ngưỡng mới
        maxVol = avgVol;                 // Cập nhật âm lượng tối đa bằng trung bình
    }
}

void CycleVisual()
{ // Hàm chuyển đổi hiệu ứng visual
    if (!digitalRead(BUTTON_2))
    {                    // Nếu nút BUTTON_2 được nhấn (LOW)
        delay(10);       // Chờ 10ms để debounce
        shuffle = false; // Tắt chế độ shuffle tự động
        visual++;        // Tăng chỉ số hiệu ứng visual
        gradient = 0;    // Đặt lại gradient
        if (visual > VISUALS)
        {                   // Nếu vượt quá số hiệu ứng
            shuffle = true; // Bật lại shuffle
            visual = 0;     // Đặt lại về hiệu ứng đầu tiên
        }
        if (visual == 1)
            memset(pos, -2, sizeof(pos)); // Nếu là Traffic, đặt lại mảng pos
        if (visual == 2 || visual == 3)
        {                                       // Nếu là VU1 hoặc VU2
            randomSeed(analogRead(0));          // Khởi tạo seed ngẫu nhiên từ A0
            dotPos = random(strip.numPixels()); // Chọn vị trí chấm ngẫu nhiên
        }
        delay(350);      // Chờ 350ms để debounce
        maxVol = avgVol; // Cập nhật âm lượng tối đa bằng trung bình
    }
    if (shuffle && millis() / 1000.0 - shuffleTime > 38 && !(gradient % 2))
    {                                    // Nếu bật shuffle và qua 38 giây
        shuffleTime = millis() / 1000.0; // Cập nhật thời điểm chuyển đổi
        visual++;                        // Tăng chỉ số hiệu ứng visual
        gradient = 0;                    // Đặt lại gradient
        if (visual > VISUALS)
            visual = 0; // Nếu vượt quá số hiệu ứng, quay lại 0
        if (visual == 1)
            memset(pos, -2, sizeof(pos)); // Nếu là Traffic, đặt lại mảng pos
        if (visual == 2 || visual == 3)
        {                                       // Nếu là VU1 hoặc VU2
            randomSeed(analogRead(0));          // Khởi tạo seed ngẫu nhiên từ A0
            dotPos = random(strip.numPixels()); // Chọn vị trí chấm ngẫu nhiên
        }
        maxVol = avgVol; // Cập nhật âm lượng tối đa bằng trung bình
    }
}

void setup()
{                                                          // Hàm khởi tạo chương trình
    Serial.begin(9600);                                    // Khởi tạo giao tiếp Serial với tốc độ 9600 baud
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Khởi tạo bộ nhận IR tại chân 7, bật đèn phản hồi
    strip.begin();                                         // Khởi tạo dải LED NeoPixel
    strip.setBrightness(brightness);                       // Đặt độ sáng ban đầu cho dải LED (150)
    strip.show();                                          // Hiển thị trạng thái ban đầu (tắt hết)

    pinMode(BUTTON_1, INPUT);     // Đặt chân BUTTON_1 (6) làm đầu vào
    pinMode(BUTTON_2, INPUT);     // Đặt chân BUTTON_2 (5) làm đầu vào
    pinMode(BUTTON_3, INPUT);     // Đặt chân BUTTON_3 (4) làm đầu vào
    digitalWrite(BUTTON_1, HIGH); // Kích hoạt pull-up nội cho BUTTON_1
    digitalWrite(BUTTON_2, HIGH); // Kích hoạt pull-up nội cho BUTTON_2
    digitalWrite(BUTTON_3, HIGH); // Kích hoạt pull-up nội cho BUTTON_3
}

void loop()
{ // Hàm vòng lặp chính
    if (mode)
    {               // Nếu ở chế độ code 1 (IR)
        handleIR(); // Xử lý tín hiệu hồng ngoại
        if (testEffectActive)
        {                       // Nếu hiệu ứng test đang chạy
            updateTestEffect(); // Cập nhật hiệu ứng test
        }
        if (ledOn)
        { // Nếu LED đang bật
            if (animationMode && animationPlaying)
            {                      // Nếu ở chế độ animation và đang phát
                updateAnimation(); // Cập nhật animation
            }
        }
    }
    else
    {                                           // Nếu ở chế độ code 2 (âm thanh)
        handleIR();                             // Vẫn kiểm tra tín hiệu IR để chuyển chế độ
        volume = (analogRead(AUDIO_PIN) - 126); // Đọc tín hiệu âm thanh từ A0, trừ 126 để chuẩn hóa
        knob = 0.9;                             // Gán giá trị cố định cho knob (0.9), không dùng KNOB_PIN
        if (volume < avgVol / 2.0 || volume < 15)
            volume = 0; // Nếu âm lượng quá thấp, đặt về 0
        else
            avgVol = (avgVol + volume) / 2.0; // Cập nhật trung bình âm lượng
        if (volume > maxVol)
            maxVol = volume; // Cập nhật âm lượng tối đa nếu vượt quá
        CyclePalette();      // Kiểm tra và chuyển đổi palette
        CycleVisual();       // Kiểm tra và chuyển đổi visual
        if (gradient > thresholds[palette])
        {                                        // Nếu gradient vượt ngưỡng palette
            gradient %= thresholds[palette] + 1; // Điều chỉnh gradient trong phạm vi
            maxVol = (maxVol + volume) / 2.0;    // Cập nhật âm lượng tối đa
        }
        if (volume - last > 10)
            avgBump = (avgBump + (volume - last)) / 2.0; // Cập nhật trung bình "bump"
        bump = (volume - last > avgBump * .9);           // Kiểm tra nếu có "bump" (tăng đột ngột)
        if (bump)
        {                                                                 // Nếu có "bump"
            avgTime = (((millis() / 1000.0) - timeBump) + avgTime) / 2.0; // Cập nhật trung bình thời gian giữa các bump
            timeBump = millis() / 1000.0;                                 // Lưu thời điểm bump hiện tại
        }
        Visualize();   // Thực hiện hiệu ứng visual
        gradient++;    // Tăng gradient để tạo hiệu ứng chuyển động
        last = volume; // Lưu âm lượng hiện tại làm âm lượng trước đó
        delay(16);     // Chờ 16ms để kiểm soát tốc độ vòng lặp (~60 FPS)
    }
}
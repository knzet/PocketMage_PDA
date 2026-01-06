
#include <globals.h>
#if !OTA_APP // POCKETMAGE_OS
static constexpr const char* TAG = "CALENDAR"; // Tag for all calls to ESP_LOG

// function prototypes
int daysInMonth(int year, int month);
void updateEventArray();

enum CalendarState { WEEK, MONTH, NEW_EVENT, VIEW_EVENT, SUN, MON, TUE, WED, THU, FRI, SAT };
CalendarState CurrentCalendarState = MONTH;

static String currentLine = "";

int monthOffsetCount = 0;
int weekOffsetCount = 0;

int currentDate = 0;
int currentMonth = 0;
int currentYear = 0;

// New Event
int newEventState = 0;
int editingEventIndex = 0;
String newEventName = "";
String newEventStartDate = "";
String newEventStartTime = "";
String newEventDuration = "";
String newEventRepeat = "";
String newEventNote = "";

std::vector<std::vector<String>> dayEvents;
std::vector<std::vector<String>> calendarEvents;

void CALENDAR_INIT() {
  currentLine = "";
  CurrentAppState = CALENDAR;
  CurrentCalendarState = MONTH;
  KB().setKeyboardState(NORMAL);
  newState = true;
  monthOffsetCount = 0;
  weekOffsetCount = 0;
}

// General Functions
// Convert "SU", "MO", "TU", etc. to 0..6 (Sun=0)
int dayStringToInt(const String &day) {
    if (day == "SU") return 0;
    if (day == "MO") return 1;
    if (day == "TU") return 2;
    if (day == "WE") return 3;
    if (day == "TH") return 4;
    if (day == "FR") return 5;
    if (day == "SA") return 6;
    return -1; // invalid
}
String repeatCodeToICS(const String &repeat) {
    String r = repeat;
    r.toUpperCase();
    if (r == "NO") return "";           // no recurrence
    if (r == "DAILY") return "FREQ=DAILY";
    if (r.startsWith("WEEKLY")) {
        // Expect "WEEKLY SU", "WEEKLY MO,WE" etc
        String days = r.substring(7);
        days.trim();
        return "FREQ=WEEKLY;BYDAY=" + days;
    }
    if (r.startsWith("MONTHLY")) {
        // Could be "MONTHLY 10" (day) or "MONTHLY 2TU" (nth weekday)
        return "FREQ=MONTHLY;BYMONTHDAY=" + r.substring(8);  // basic, extend later
    }
    if (r.startsWith("YEARLY")) {
        return "FREQ=YEARLY";  // extend later with BYMONTH/BYMONTHDAY
    }
    return "";  // fallback
}
String ICSRRULEtoApp(const String &rrule) {
    if (rrule.length() == 0) return "NO";

    String r = rrule;
    r.toUpperCase();

    if (r.startsWith("FREQ=DAILY")) return "DAILY";

    if (r.startsWith("FREQ=WEEKLY")) {
        int byDayIdx = r.indexOf("BYDAY=");
        if (byDayIdx >= 0) {
            String days = r.substring(byDayIdx + 6);
            days.trim();
            return "WEEKLY " + days;
        }
        return "WEEKLY";
    }

    if (r.startsWith("FREQ=MONTHLY")) {
        int byMonthDayIdx = r.indexOf("BYMONTHDAY=");
        if (byMonthDayIdx >= 0) {
            String day = r.substring(byMonthDayIdx + 11);
            day.trim();
            return "MONTHLY " + day;
        }
        return "MONTHLY";
    }

    if (r.startsWith("FREQ=YEARLY")) return "YEARLY";

    return "NO"; // fallback
}

// Parse YYYYMMDD string to DateTime
DateTime parseYYYYMMDD(const String &yyyymmdd) {
    int year  = yyyymmdd.substring(0, 4).toInt();
    int month = yyyymmdd.substring(4, 6).toInt();
    int day   = yyyymmdd.substring(6, 8).toInt();
    return DateTime(year, month, day);
}

// Compare two times in HHMM or HH:MM format
bool isAfter(const String &startTime, const String &currentTime) {
    String s = startTime;
    String c = currentTime;
    s.replace(":", "");
    c.replace(":", "");
    return s.toInt() >= c.toInt();
}

String intToYYYYMMDD(int year_, int month_, int date_) {
  String y = String(year_);
  String m = (month_ < 10 ? "0" : "") + String(month_);
  String d = (date_ < 10 ? "0" : "") + String(date_);
  return y + m + d;
}

String getMonthName(int month) {
  switch(month) {
    case 1: return "Jan";
    case 2: return "Feb";
    case 3: return "Mar";
    case 4: return "Apr";
    case 5: return "May";
    case 6: return "Jun";
    case 7: return "Jul";
    case 8: return "Aug";
    case 9: return "Sep";
    case 10: return "Oct";
    case 11: return "Nov";
    case 12: return "Dec";
    default: return "ERR";
  }
}

int getDayOfWeek(int year, int month, int day) {
  if (month < 3) {
    month += 12;
    year -= 1;
  }

  int K = year % 100;
  int J = year / 100;

  int h = (day + 13*(month + 1)/5 + K + K/4 + J/4 + 5*J) % 7;

  // Convert Zeller’s output to: 0 = Sunday, ..., 6 = Saturday
  int d = (h + 6) % 7;
  return d;
}

int stringToPositiveInt(String input) {
  input.trim();
  if (input.length() == 0) return -1;

  for (int i = 0; i < input.length(); i++) {
    if (!isDigit(input[i])) return -1;
  }

  return input.toInt();
}

int daysInMonth(int year, int month) {
  if (month == 2) {
    // Leap year
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  } else if (month == 4 || month == 6 || month == 9 || month == 11) {
    return 30;
  } else {
    return 31;
  }
}

void commandSelectMonth(String command) {
  command.toLowerCase();

  const char* monthNames[] = {
    "jan", "feb", "mar", "apr", "may", "jun",
    "jul", "aug", "sep", "oct", "nov", "dec"
  };

  if (command == "n") {
    CurrentCalendarState = NEW_EVENT;

    // Initialize Stuff
    newEventState = 0;
    newEventName = "";
    newEventStartDate = "";
    newEventStartTime = "";
    newEventDuration = "";
    newEventRepeat = "";
    newEventNote = "";
    currentLine     = "";

    newState        = true;
    KB().setKeyboardState(NORMAL);
    return;
  }

  // Check if command starts with a 3-letter month
  else if (command.length() >= 4) {
    String prefix = command.substring(0, 3);
    String yearPart = command.substring(4);
    yearPart.trim();

    for (int i = 0; i < 12; i++) {
      if (prefix == monthNames[i]) {
        int yearInt = stringToInt(yearPart);
        if (yearInt == -1 || yearInt < 1970 || yearInt > 2200) {
          OLED().oledWord("Invalid");
          delay(500);
          return;
        }

        currentMonth = i + 1;        // 1-indexed month
        currentYear = yearInt;
        newState = true;

        // Update monthOffsetCount relative to now
        DateTime now = CLOCK().nowDT();
        int currentAbsMonth = now.year() * 12 + now.month();
        int targetAbsMonth = currentYear * 12 + currentMonth;
        monthOffsetCount = targetAbsMonth - currentAbsMonth;

        return;
      }
    }
  }

  // Check if command is in YYYYMMDD format
  else if (command.length() == 8 && stringToPositiveInt(command) != -1) {
    int year = command.substring(0, 4).toInt();
    int month = command.substring(4, 6).toInt();
    int date = command.substring(6, 8).toInt();

    if (year < 1970 || year > 2200 || month < 1 || month > 12 || date < 1 || date > daysInMonth(month, year)) {
      OLED().oledWord("Invalid");
      delay(500);
      return;
    }

    currentYear = year;
    currentMonth = month;
    currentDate = date;

    DateTime now = CLOCK().nowDT();
    int currentAbsMonth = now.year() * 12 + now.month();
    int targetAbsMonth = currentYear * 12 + currentMonth;
    monthOffsetCount = targetAbsMonth - currentAbsMonth;

    int dayOfWeek = getDayOfWeek(currentYear, currentMonth, currentDate);

    switch (dayOfWeek) {
      case 0:
        CurrentCalendarState = SUN;
        break;
      case 1:
        CurrentCalendarState = MON;
        break;
      case 2:
        CurrentCalendarState = TUE;
        break;
      case 3:
        CurrentCalendarState = WED;
        break;
      case 4:
        CurrentCalendarState = THU;
        break;
      case 5:
        CurrentCalendarState = FRI;
        break;
      case 6:
        CurrentCalendarState = SAT;
        break;
    }

    newState        = true;
    KB().setKeyboardState(NORMAL);
    return;
  }

  // Check if user entered a numeric day (for current month)
  else {
    int intDay = stringToPositiveInt(command);
    DateTime now = CLOCK().nowDT();
    if (intDay == -1 || intDay > daysInMonth(currentMonth, currentYear)) {
      OLED().oledWord("Invalid");
      delay(500);
      return;
    }
    else {
      currentDate = intDay;

      int dayOfWeek = getDayOfWeek(currentYear, currentMonth, currentDate);

      switch (dayOfWeek) {
        case 0:
          CurrentCalendarState = SUN;
          break;
        case 1:
          CurrentCalendarState = MON;
          break;
        case 2:
          CurrentCalendarState = TUE;
          break;
        case 3:
          CurrentCalendarState = WED;
          break;
        case 4:
          CurrentCalendarState = THU;
          break;
        case 5:
          CurrentCalendarState = FRI;
          break;
        case 6:
          CurrentCalendarState = SAT;
          break;
      }

      newState        = true;
      KB().setKeyboardState(NORMAL);
      return;
    }
  }
}

void commandSelectWeek(String command) {
  command.toLowerCase();

  if (command == "n") {
    CurrentCalendarState = NEW_EVENT;

    // Initialize Stuff
    newEventState = 0;
    newEventName = "";
    newEventStartDate = "";
    newEventStartTime = "";
    newEventDuration = "";
    newEventRepeat = "";
    newEventNote = "";
    currentLine     = "";

    newState        = true;
    KB().setKeyboardState(NORMAL);
    return;
  }
  // Commands for each day
  else if (command == "sun" || command == "su") {
    CurrentCalendarState = SUN;

    DateTime now = CLOCK().nowDT();
    int todayDOW = getDayOfWeek(now.year(), now.month(), now.day()); 
    DateTime currentSunday = now - TimeSpan(todayDOW, 0, 0, 0);
    DateTime viewedSunday = currentSunday + TimeSpan(weekOffsetCount * 7, 0, 0, 0);

    currentDate  = viewedSunday.day();
    currentMonth = viewedSunday.month();
    currentYear  = viewedSunday.year();

    newState = true;
    KB().setKeyboardState(NORMAL);
  }

  else if (command == "mon" || command == "mo") {
    CurrentCalendarState = MON;

    DateTime now = CLOCK().nowDT();
    int todayDOW = getDayOfWeek(now.year(), now.month(), now.day());
    DateTime currentSunday = now - TimeSpan(todayDOW, 0, 0, 0);
    DateTime viewedMonday = currentSunday + TimeSpan(weekOffsetCount * 7 + 1, 0, 0, 0);

    currentDate  = viewedMonday.day();
    currentMonth = viewedMonday.month();
    currentYear  = viewedMonday.year();

    newState = true;
    KB().setKeyboardState(NORMAL);
  }

  else if (command == "tue" || command == "tu") {
    CurrentCalendarState = TUE;

    DateTime now = CLOCK().nowDT();
    int todayDOW = getDayOfWeek(now.year(), now.month(), now.day());
    DateTime currentSunday = now - TimeSpan(todayDOW, 0, 0, 0);
    DateTime viewedTuesday = currentSunday + TimeSpan(weekOffsetCount * 7 + 2, 0, 0, 0);

    currentDate  = viewedTuesday.day();
    currentMonth = viewedTuesday.month();
    currentYear  = viewedTuesday.year();

    newState = true;
    KB().setKeyboardState(NORMAL);
  }

  else if (command == "wed" || command == "we") {
    CurrentCalendarState = WED;

    DateTime now = CLOCK().nowDT();
    int todayDOW = getDayOfWeek(now.year(), now.month(), now.day());
    DateTime currentSunday = now - TimeSpan(todayDOW, 0, 0, 0);
    DateTime viewedWednesday = currentSunday + TimeSpan(weekOffsetCount * 7 + 3, 0, 0, 0);

    currentDate  = viewedWednesday.day();
    currentMonth = viewedWednesday.month();
    currentYear  = viewedWednesday.year();

    newState = true;
    KB().setKeyboardState(NORMAL);
  }

  else if (command == "thu" || command == "th") {
    CurrentCalendarState = THU;

    DateTime now = CLOCK().nowDT();
    int todayDOW = getDayOfWeek(now.year(), now.month(), now.day());
    DateTime currentSunday = now - TimeSpan(todayDOW, 0, 0, 0);
    DateTime viewedThursday = currentSunday + TimeSpan(weekOffsetCount * 7 + 4, 0, 0, 0);

    currentDate  = viewedThursday.day();
    currentMonth = viewedThursday.month();
    currentYear  = viewedThursday.year();

    newState = true;
    KB().setKeyboardState(NORMAL);
  }

  else if (command == "fri" || command == "fr") {
    CurrentCalendarState = FRI;

    DateTime now = CLOCK().nowDT();
    int todayDOW = getDayOfWeek(now.year(), now.month(), now.day());
    DateTime currentSunday = now - TimeSpan(todayDOW, 0, 0, 0);
    DateTime viewedFriday = currentSunday + TimeSpan(weekOffsetCount * 7 + 5, 0, 0, 0);

    currentDate  = viewedFriday.day();
    currentMonth = viewedFriday.month();
    currentYear  = viewedFriday.year();

    newState = true;
    KB().setKeyboardState(NORMAL);
  }

  else if (command == "sat" || command == "sa") {
    CurrentCalendarState = SAT;

    DateTime now = CLOCK().nowDT();
    int todayDOW = getDayOfWeek(now.year(), now.month(), now.day());
    DateTime currentSunday = now - TimeSpan(todayDOW, 0, 0, 0);
    DateTime viewedSaturday = currentSunday + TimeSpan(weekOffsetCount * 7 + 6, 0, 0, 0);

    currentDate  = viewedSaturday.day();
    currentMonth = viewedSaturday.month();
    currentYear  = viewedSaturday.year();

    newState = true;
    KB().setKeyboardState(NORMAL);
  }
}

void commandSelectDay(String command) {
  command.toLowerCase();

  if (command == "n") {
    CurrentCalendarState = NEW_EVENT;

    // Initialize new blank event
    newEventState     = 0;
    newEventName      = "";
    newEventStartDate = intToYYYYMMDD(currentYear, currentMonth, currentDate);
    newEventStartTime = "";
    newEventDuration  = "";
    newEventRepeat    = "";
    newEventNote      = "";
    currentLine       = "";

    newState          = true;
    KB().setKeyboardState(NORMAL);
    return;
  }

  // Check if the command is a single digit referring to a specific event
  if (command.length() == 1 && isDigit(command.charAt(0))) {
    int index = command.toInt() - 1;

    if (index >= 0 && index < dayEvents.size()) {
      std::vector<String>& evt = dayEvents[index];

      editingEventIndex = index;
      newEventState     = -1;
      newEventName      = evt[0];
      newEventStartDate = evt[1];
      newEventStartTime = evt[2];
      newEventDuration  = evt[3];
      newEventRepeat    = evt[4];
      newEventNote      = evt[5];
      currentLine       = "";

      CurrentCalendarState = VIEW_EVENT;
      KB().setKeyboardState(NORMAL);
      newState             = true;
    }
  }
}
// Calculate duration in minutes between two ICS datetimes (YYYYMMDDTHHMMSS)
int calculateDurationMinutes(const String& dtStart, const String& dtEnd) {
    // Parse start
    int sYear  = dtStart.substring(0,4).toInt();
    int sMonth = dtStart.substring(4,6).toInt();
    int sDay   = dtStart.substring(6,8).toInt();
    int sHour  = dtStart.substring(9,11).toInt();
    int sMin   = dtStart.substring(11,13).toInt();

    // Parse end
    int eYear  = dtEnd.substring(0,4).toInt();
    int eMonth = dtEnd.substring(4,6).toInt();
    int eDay   = dtEnd.substring(6,8).toInt();
    int eHour  = dtEnd.substring(9,11).toInt();
    int eMin   = dtEnd.substring(11,13).toInt();

    // Convert to minutes since epoch (simple)
    long startMinutes = (((sYear*12 + sMonth)*31 + sDay)*24 + sHour)*60 + sMin;
    long endMinutes   = (((eYear*12 + eMonth)*31 + eDay)*24 + eHour)*60 + eMin;

    long diff = endMinutes - startMinutes;
    if (diff < 0) diff = 0;
    return (int)diff;
}
bool isOnOrAfter(const String &date, const String &startDate) {
    return date >= startDate;  // works because YYYYMMDD
}

int checkEvents(const String &YYYYMMDD, bool silent) {
    dayEvents.clear();
    int count = 0;

    // Only load ICS files once per app run
    static bool calendarEventsLoaded = false;
    if (!calendarEventsLoaded) {
        updateEventArray();  // populate calendarEvents
        calendarEventsLoaded = true;
    }

    for (const auto& e : calendarEvents) {
      String eventDate = e[1];  // DTSTART (YYYYMMDD)
      String repeat = e[4];

      bool includeEvent = false;

      if (YYYYMMDD < eventDate) {
        continue;
      }

      if (eventDate == YYYYMMDD) {
        includeEvent = true;
      } else if (repeat.startsWith("DAILY")) {
        includeEvent = true;
      } else if (repeat.startsWith("WEEKLY")) {
        // repeat format: "WEEKLY FR" or "WEEKLY MO,WE"
        String days = repeat.substring(7);  // after "WEEKLY"
        days.trim();

        int pos = 0;
        while (pos < days.length()) {
          int comma = days.indexOf(',', pos);
          if (comma == -1)
            comma = days.length();

          String token = days.substring(pos, comma);
          token.trim();

          int y = YYYYMMDD.substring(0, 4).toInt();
          int m = YYYYMMDD.substring(4, 6).toInt();
          int d = YYYYMMDD.substring(6, 8).toInt();
          int dow = getDayOfWeek(y, m, d);  // 0=Sun ... 6=Sat
          const char* dows[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
          String dowStr = dows[dow];
          if (token == dowStr) {
            includeEvent = true;
            break;
          }

          pos = comma + 1;
        }
      }

      if (includeEvent) {
        dayEvents.push_back(e);
        if (++count >= 7)
          break;
      }
    }

    if (!silent) ESP_LOGI(TAG, "checkEvents(): %d events on %s", count, YYYYMMDD.c_str());
    return count;
}

void drawCalendarMonth(int monthOffset) {
  int GRID_X =  7;     // X offset of first cell
  int GRID_Y = 49;     // Y offset of first row
  int CELL_W = 44;     // Width of each cell
  int CELL_H = 27;     // Height of each cell

  DateTime now = CLOCK().nowDT();

  // Step 1: Calculate target month/year
  int month = now.month() + monthOffset;
  int year = now.year();
  while (month > 12) { month -= 12; year++; }
  while (month < 1)  { month += 12; year--; }

  currentMonth = month;
  currentYear = year;

  // Draw Background
  EINK().drawStatusBar(getMonthName(currentMonth) + " " + String(currentYear)+ " | Type a Date:");
  display.drawBitmap(0, 0, calendar_allArray[1], 320, 218, GxEPD_BLACK);

  // Step 2: Day of the week for the 1st of the month (0 = Sun, 6 = Sat)
  DateTime firstDay(year, month, 1);
  int startDay = firstDay.dayOfTheWeek();  // 0–6, Sun to Sat

  // Step 3: Number of days in the month
  int nextYear  = (month == 12) ? (year + 1) : year;
  int nextMonth = (month == 12) ? 1 : (month + 1);

  int daysInMonth = (DateTime(nextYear, nextMonth, 1) - DateTime(year, month, 1)).days();

  // Step 4: Blank out leading days
  for (int i = 0; i < startDay; ++i) {
    int x = GRID_X + i * CELL_W;
    int y = GRID_Y;
    display.fillRect(x, y, CELL_W, CELL_H, GxEPD_WHITE);
  }

  // Step 5: Blank out trailing days
  int totalBoxes = 42;  // 7x6 grid
  int trailingStart = startDay + daysInMonth;
  for (int i = trailingStart; i < totalBoxes; ++i) {
    int row = i / 7;
    int col = i % 7;
    int x = GRID_X + col * CELL_W;
    int y = GRID_Y + row * CELL_H;
    display.fillRect(x, y, CELL_W, CELL_H, GxEPD_WHITE);
  }
  // Step 6: Draw day numbers and events
  for (int i = 0; i < daysInMonth; ++i) {
    int dayIndex = i + startDay;     // total box index in the 7x6 grid
    int row = dayIndex / 7;
    int col = dayIndex % 7;

    int x = GRID_X + col * CELL_W;
    int y = GRID_Y + row * CELL_H;

    int dayNum = i + 1;  // 1-based day number

    // Current day
    if (dayNum == now.day() && monthOffset == 0) {
      display.setFont(&FreeSerifBold9pt7b);
    }
    else display.setFont(&FreeSerif9pt7b);
    
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x + 6, y + 15); 
    display.print(dayNum);

    // Draw icon if there are events on day

    String YYYYMMDD = intToYYYYMMDD(year, month, dayNum);
    // Pad month and dayNum with leading zeros
    /*String paddedMonth = (month < 10 ? "0" : "") + String(month);
    String paddedDay   = (dayNum < 10 ? "0" : "") + String(dayNum);

    // Format date as YYYYMMDD
    String YYYYMMDD = String(year) + paddedMonth + paddedDay;*/

    int numEvents = checkEvents(YYYYMMDD, true);

    // Events found
    if (numEvents > 2) {
      display.setFont(&Font5x7Fixed);
      display.setCursor(x + 32, y + 16);
      display.print(String(numEvents));
    }
    else if (numEvents > 1) {
      // More than 1 event
      display.drawBitmap(x + 29, y + 8, _eventMarker1, 10, 10, GxEPD_BLACK);
    }
    else if (numEvents > 0) {
      // One event exists
      display.drawBitmap(x + 29, y + 8, _eventMarker0, 10, 10, GxEPD_BLACK);
    }
  }
}

void drawCalendarWeek(int weekOffset) {
  EINK().drawStatusBar("Type Sun, etc. or (N)ew");
  display.drawBitmap(0, 0, calendar_allArray[0], 320, 218, GxEPD_BLACK);

  // Get current date
  DateTime now = CLOCK().nowDT();
  int year = now.year();
  int month = now.month();
  int day = now.day();
  int dow = now.dayOfTheWeek();  // 0 = Sunday

  // Calculate how many days to go back to get to Sunday, adjusted by weekOffset
  int totalOffset = -dow + (weekOffset * 7);

  for (int i = 0; i < 7; i++) {
    // Compute day offset from today
    int offset = totalOffset + i;

    // Convert (year, month, day + offset) into a new date
    int y = year;
    int m = month;
    int d = day + offset;

    // Normalize date forward/backward
    while (d <= 0) {
      m--;
      if (m < 1) {
        m = 12;
        y--;
      }
      d += daysInMonth(m, y);
    }
    while (d > daysInMonth(m, y)) {
      d -= daysInMonth(m, y);
      m++;
      if (m > 12) {
        m = 1;
        y++;
      }
    }

    // Format YYYYMMDD
    String YYYYMMDD = intToYYYYMMDD(y, m, d);
    /*String YYYYMMDD = String(y) +
                      (m < 10 ? "0" : "") + String(m) +
                      (d < 10 ? "0" : "") + String(d);*/

    // Draw date
    display.setFont(&FreeSerif9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(9 + (i * 44), 62);
    String dateStr = String(m) + "/" + String(d);
    display.print(dateStr);

    // Load and draw events
    int eventCount = checkEvents(YYYYMMDD, false);
    if (eventCount > 6) eventCount = 6;

    // Blank out extra space
    display.fillRect(9 + (i * 44), 71 + (eventCount * 23), 39, ((6 - eventCount) * 23), GxEPD_WHITE);

    for (int j = 0; j < eventCount; j++) {
      String startTime = dayEvents[j][2];
      // Indicator for repeat events
      if (dayEvents[j][4] != "NO") startTime = ":: " + startTime;
      String eventName = dayEvents[j][0].substring(0, 6);

      // Print Start Time
      display.setFont(&Font3x7FixedNum);
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(12 + (i * 44), 80 + (j * 23));
      display.print(startTime);

      // Print Event Name
      display.setFont(&Font5x7Fixed);
      display.setCursor(12 + (i * 44), 89 + (j * 23));
      display.print(eventName);
    }
  }
}

// ics import/export
void expandRecurringEvents(int year, int month, int day) {
    dayEvents.clear();

    for (auto &e : calendarEvents) {
        String eventName  = e[0];
        String startDate  = e[1]; // YYYYMMDD
        String startTime  = e[2]; // HH:MM
        String duration   = e[3];
        String repeatRule = e[4];
        String note       = e[5];

        // Add non-repeating event if it matches today
        if (startDate == intToYYYYMMDD(year, month, day)) {
            dayEvents.push_back({eventName, startDate, startTime, duration, repeatRule, note});
        }

        // Handle weekly recurrence
        if (repeatRule.startsWith("WEEKLY")) {
            // extract BYDAY
            String byDay = repeatRule.substring(7); // e.g., "FR"
            int targetDOW = dayStringToInt(byDay); // 0=SUN..6=SAT
            int todayDOW = getDayOfWeek(year, month, day);

            // Check if this date is a recurrence
            DateTime start = parseYYYYMMDD(startDate);
            String startHHMM = (start.hour() < 10 ? "0" : "") + String(start.hour()) +
                               (start.minute() < 10 ? "0" : "") + String(start.minute());
            DateTime now = CLOCK().nowDT();
            String todayHHMM = (now.hour() < 10 ? "0" : "") + String(now.hour()) +
                               (now.minute() < 10 ? "0" : "") + String(now.minute());

            if (todayDOW == targetDOW && isAfter(startHHMM, todayHHMM)) {
                dayEvents.push_back({eventName, intToYYYYMMDD(year, month, day),
                                     startTime, duration, repeatRule, note});
            }
        }

        // TODO: add DAILY, MONTHLY, YEARLY if needed
    }
}

bool parseICSDateTime(const String& line,
                      String& outDate,  // YYYYMMDD
                      String& outTime   // HH:MM
) {
  // Expected formats:
  // DTSTART:YYYYMMDD
  // DTSTART:YYYYMMDDTHHMMSS
  // DTSTART:YYYYMMDDTHHMM

  int colon = line.indexOf(':');
  if (colon < 0)
    return false;

  String value = line.substring(colon + 1);
  value.trim();

  if (value.length() < 8)
    return false;

  // Date
  outDate = value.substring(0, 8);

  // Time (optional)
  if (value.length() >= 13 && value.charAt(8) == 'T') {
    String hh = value.substring(9, 11);
    String mm = value.substring(11, 13);
    outTime = hh + ":" + mm;
  } else {
    outTime = "00:00";
  }

  return true;
}
bool computeICSDuration(const String& startLine,  // DTSTART:...
                        const String& endLine,    // DTEND:...
                        String& outDuration       // H:MM
) {
  String startDate, startTime;
  String endDate, endTime;

  if (!parseICSDateTime(startLine, startDate, startTime))
    return false;
  if (!parseICSDateTime(endLine, endDate, endTime))
    return false;

  // Only support same-day events for now
  if (startDate != endDate)
    return false;

  int sh = startTime.substring(0, 2).toInt();
  int sm = startTime.substring(3, 5).toInt();
  int eh = endTime.substring(0, 2).toInt();
  int em = endTime.substring(3, 5).toInt();

  int startMinutes = sh * 60 + sm;
  int endMinutes = eh * 60 + em;

  if (endMinutes <= startMinutes)
    return false;

  int diff = endMinutes - startMinutes;
  int hours = diff / 60;
  int minutes = diff % 60;

  outDuration = String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes);
  return true;
}
bool parseICSRRule(const String& rruleLine, String& outRepeat) {
  outRepeat = "NO";

  if (!rruleLine.startsWith("RRULE:"))
    return false;

  String rule = rruleLine.substring(6);
  rule.toUpperCase();

  // DAILY
  if (rule.indexOf("FREQ=DAILY") >= 0) {
    outRepeat = "DAILY";
    return true;
  }

  // WEEKLY
  if (rule.indexOf("FREQ=WEEKLY") >= 0) {
    int bydayIdx = rule.indexOf("BYDAY=");
    if (bydayIdx >= 0) {
      String days = rule.substring(bydayIdx + 6);
      int semi = days.indexOf(';');
      if (semi >= 0)
        days = days.substring(0, semi);

      // days.replace(",", ""); 
      outRepeat = "WEEKLY " + days;
      return true;
    }
  }

  // MONTHLY
  if (rule.indexOf("FREQ=MONTHLY") >= 0) {
    int bymonthdayIdx = rule.indexOf("BYMONTHDAY=");
    if (bymonthdayIdx >= 0) {
      String day = rule.substring(bymonthdayIdx + 11);
      int semi = day.indexOf(';');
      if (semi >= 0)
        day = day.substring(0, semi);

      outRepeat = "MONTHLY " + day;
      return true;
    }

    int bydayIdx = rule.indexOf("BYDAY=");
    if (bydayIdx >= 0) {
      String code = rule.substring(bydayIdx + 6);
      int semi = code.indexOf(';');
      if (semi >= 0)
        code = code.substring(0, semi);

      outRepeat = "MONTHLY " + code;
      return true;
    }
  }

  // YEARLY
  if (rule.indexOf("FREQ=YEARLY") >= 0) {
    int bymonthIdx = rule.indexOf("BYMONTH=");
    int bymonthdayIdx = rule.indexOf("BYMONTHDAY=");

    if (bymonthIdx >= 0 && bymonthdayIdx >= 0) {
      int month = rule.substring(bymonthIdx + 8).toInt();
      int day = rule.substring(bymonthdayIdx + 11).toInt();

      const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                              "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

      if (month >= 1 && month <= 12) {
        outRepeat = "YEARLY " + String(months[month - 1]) + (day < 10 ? "0" : "") + String(day);
        return true;
      }
    }
  }

  return false;
}
void parseICSEvent(const std::vector<String>& icsLines) {
    std::vector<String> eventLines;
    bool inEvent = false;

    for (const auto& line : icsLines) {
        if (line == "BEGIN:VEVENT") {
            inEvent = true;
            eventLines.clear();
        }

        if (inEvent) {
            eventLines.push_back(line);
        }

        if (line == "END:VEVENT" && inEvent) {
            // Parse this VEVENT into calendarEvents
            String eventName, startDate, startTime, duration, repeat, note;

            for (const auto& eline : eventLines) {
                if (eline.startsWith("SUMMARY:")) {
                    eventName = eline.substring(8);
                } else if (eline.startsWith("DTSTART:")) {
                    String dt = eline.substring(8); // YYYYMMDDTHHMMSS
                    startDate = dt.substring(0, 8);
                    startTime = dt.substring(9, 13); // HHMM
                } else if (eline.startsWith("DTEND:")) {
                    String dtEnd = eline.substring(6);
                    int durMins = calculateDurationMinutes(startDate + "T" + startTime + "00", dtEnd);
                    duration = String(durMins) + "m";
                } else if (eline.startsWith("RRULE:")) {
                  parseICSRRule(eline, repeat);
                } else if (eline.startsWith("DESCRIPTION:")) {
                  note = eline.substring(12);
                }
            }

            // Push to calendarEvents vector
            calendarEvents.push_back({eventName, startDate, startTime, duration, repeat, note});

            inEvent = false; // done with this event
        }
    }
}
void parseICSFile(const String &filename) {
    SDActive = true;
    File file = SD_MMC.open(filename, FILE_READ);
    if (!file) {
        ESP_LOGE(TAG, "Failed to open ICS file: %s", filename.c_str());
        return;
    }

    std::vector<String> veventLines;
    bool inEvent = false;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        if (line == "BEGIN:VEVENT") {
            inEvent = true;
            veventLines.clear();
        } else if (line == "END:VEVENT") {
            inEvent = false;
            parseICSEvent(veventLines); // <-- parse one VEVENT at a time
        } else if (inEvent) {
            veventLines.push_back(line);
        }
    }

    file.close();
    SDActive = false;
}

String calculateDTEnd(const String &startDate, const String &startTime, const String &duration) {
  // Parse start date
  int year  = startDate.substring(0, 4).toInt();
  int month = startDate.substring(4, 6).toInt();
  int day   = startDate.substring(6, 8).toInt();

  // Parse start time
  int hour   = startTime.substring(0, 2).toInt();
  int minute = startTime.substring(3, 5).toInt();

  // Parse duration
  int durHours = 0;
  int durMinutes = 0;
  if (duration.endsWith("m")) {
    durMinutes = duration.substring(0, duration.length() - 1).toInt();
  } else if (duration.indexOf(':') > 0) {
    int colon = duration.indexOf(':');
    durHours = duration.substring(0, colon).toInt();
    durMinutes = duration.substring(colon + 1).toInt();
  }

  // Add duration to start time
  minute += durMinutes;
  hour += durHours + minute / 60;
  minute = minute % 60;
  day += hour / 24;
  hour = hour % 24;

  // Adjust month/year for overflow
  int daysInThisMonth = daysInMonth(year, month);
  while (day > daysInThisMonth) {
    day -= daysInThisMonth;
    month++;
    if (month > 12) {
      month = 1;
      year++;
    }
    daysInThisMonth = daysInMonth(year, month);
  }

  // Format DTEND YYYYMMDDTHHMMSS
  char buf[17];
  snprintf(buf, sizeof(buf), "%04d%02d%02dT%02d%02d%02d",
           year, month, day, hour, minute, 0);
  return String(buf);
}

// end ics import/export

// Event Data Management
// 
#pragma message "TODO: Migrate to a better/global file management system"
void updateEventArray() {
  ESP_LOGE(TAG, "updateEventsArray");
  ESP_LOGI(TAG, "IUPDATE events array");
  SDActive = true;
  pocketmage::setCpuSpeed(240);
  delay(50);

  calendarEvents.clear();

  File eventsDir = SD_MMC.open("/sys/events");
  if (!eventsDir) {
    ESP_LOGE(TAG, "Failed to open /sys/events directory");
    SDActive = false;
    return;
  }

  File file = eventsDir.openNextFile();
  while (file) {
    if (!file.isDirectory() && file.name()[0] != '\0') {
      String filename = file.name();
      if (!filename.startsWith("/")) {
        filename = "/sys/events/" + filename;
      }

      if (filename.endsWith(".ics")) {
        ESP_LOGI(TAG, "Parsing ICS file: %s", filename.c_str());

        File f = SD_MMC.open(filename.c_str(), FILE_READ);
        if (!f) {
          ESP_LOGE(TAG, "Failed to open ICS file: %s", filename.c_str());
        } else {
          std::vector<String> icsLines;
          while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) {
              icsLines.push_back(line);
            }
          }
          f.close();

          parseICSEvent(icsLines);
        }

        delay(5);
      }
    }
    file = eventsDir.openNextFile();
  }

  eventsDir.close();

  if (SAVE_POWER)
    pocketmage::setCpuSpeed(POWER_SAVE_FREQ);
  SDActive = false;

  ESP_LOGI(TAG, "updateEventArray(): loaded %d events from ICS", (int)calendarEvents.size());
}


String calculateEndTime(const String &startDate, const String &startTime, int durationMinutes) {
    // Parse start date
    int year  = startDate.substring(0, 4).toInt();
    int month = startDate.substring(4, 6).toInt();
    int day   = startDate.substring(6, 8).toInt();

    // Parse start time
    int hour = 0;
    int min  = 0;
    if (startTime.length() >= 4) {
        hour = startTime.substring(0, 2).toInt();
        min  = startTime.substring(2, 4).toInt();
    }

    // Add duration
    min += durationMinutes;
    while (min >= 60) { min -= 60; hour++; }
    while (hour >= 24) { hour -= 24; day++; }

    // Adjust day/month/year for overflow
    int daysInMonthArr[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    // Leap year
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) daysInMonthArr[2] = 29;

    while (day > daysInMonthArr[month]) {
        day -= daysInMonthArr[month];
        month++;
        if (month > 12) { month = 1; year++; }
    }

    // Build YYYYMMDDTHHMMSS string
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d%02d%02dT%02d%02d%02d", year, month, day, hour, min, 0);
    return String(buf);
}

void sortEventsByDate(std::vector<std::vector<String>> &calendarEvents) {
  std::sort(calendarEvents.begin(), calendarEvents.end(), [](const std::vector<String> &a, const std::vector<String> &b) {
    return a[1] < b[1]; // Compare dueDate strings
  });
}

void updateEventsFile() {
    SDActive = true;
    pocketmage::setCpuSpeed(240);
    delay(50);

    const char* filename = "/sys/events/calendar.ics";
    File f = SD_MMC.open(filename, FILE_WRITE);
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", filename);
        SDActive = false;
        return;
    }

    // Header
    f.println("BEGIN:VCALENDAR");
    f.println("PRODID:-//PocketMage//CalendarApp//EN");
    f.println("VERSION:2.0");

    for (size_t i = 0; i < calendarEvents.size(); i++) {
        const auto &e = calendarEvents[i];

        String eventName = e[0];
        String startDate = e[1];   // YYYYMMDD
        String startTime = e[2];   // HHMM
        String duration  = e[3];   // minutes as string
        String repeat    = e[4];   // "DAILY", "WEEKLY FR", "NO"
        String note      = e[5];

        // Format DTSTART / DTEND as YYYYMMDDTHHMMSS
        String dtStart = startDate + "T" + (startTime.length() == 4 ? startTime : "0000") + "00";
        int durMin = duration.toInt();
        String dtEnd = calculateEndTime(startDate, startTime, durMin); // returns YYYYMMDDTHHMMSS

        // UID & DTSTAMP
        time_t now = time(nullptr);
        struct tm t;
        gmtime_r(&now, &t);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", &t);
        String dtStamp = buf;
        String uid = "PM_" + String(i) + "_" + dtStamp + "@pocketmage";

        f.println("BEGIN:VEVENT");
        f.println("UID:" + uid);
        f.println("DTSTAMP:" + dtStamp);
        f.println("SUMMARY:" + eventName);
        f.println("DTSTART:" + dtStart);
        f.println("DTEND:" + dtEnd);
        if (repeat != "NO") {
          String rrule = repeatCodeToICS(repeat);
          if (rrule.length() > 0) {
            f.println("RRULE:" + rrule);
          }
        }
        f.println("DESCRIPTION:" + note);
        f.println("END:VEVENT");
    }

    f.println("END:VCALENDAR");
    f.close();

    if (SAVE_POWER) pocketmage::setCpuSpeed(POWER_SAVE_FREQ);
    SDActive = false;
}

void addEvent(String eventName, String startDate, String startTime , String duration, String repeat, String note) {
  String eventInfo = eventName+"|"+startDate+"|"+startTime +"|"+duration+"|"+repeat+"|"+note;
  updateEventArray();
  calendarEvents.push_back({eventName, startDate, startTime , duration, repeat, note});
  sortEventsByDate(calendarEvents);
  updateEventsFile();
}

void deleteEvent(int index) {
  if (index >= 0 && index < calendarEvents.size()) {
    calendarEvents.erase(calendarEvents.begin() + index);
  }
}

void deleteEventByIndex(int indexToDelete) {
  if (indexToDelete >= 0 && indexToDelete < dayEvents.size()) {
    std::vector<String> targetEvent = dayEvents[indexToDelete];

    // Remove from dayEvents
    dayEvents.erase(dayEvents.begin() + indexToDelete);

    // Remove matching event from calendarEvents
    for (int i = 0; i < calendarEvents.size(); i++) {
      if (calendarEvents[i] == targetEvent) {
        calendarEvents.erase(calendarEvents.begin() + i);
        break;  // Only remove the first match
      }
    }
  }
}

void updateEventByIndex(int indexToUpdate) {
  if (indexToUpdate >= 0 && indexToUpdate < dayEvents.size()) {
    std::vector<String> oldEvent = dayEvents[indexToUpdate];

    // New event data
    std::vector<String> updatedEvent = {
      newEventName,
      newEventStartDate,
      newEventStartTime,
      newEventDuration,
      newEventRepeat,
      newEventNote
    };

    // Update dayEvents
    dayEvents[indexToUpdate] = updatedEvent;

    // Find and update matching event in calendarEvents
    for (int i = 0; i < calendarEvents.size(); i++) {
      if (calendarEvents[i] == oldEvent) {
        calendarEvents[i] = updatedEvent;
        break;  // Stop after first match
      }
    }
  }
}


// Loops
void processKB_CALENDAR() {
  int currentMillis = millis();
   DateTime now = CLOCK().nowDT();

   switch (CurrentCalendarState) {
     case MONTH:
       if (currentMillis - KBBounceMillis >= KB_COOLDOWN) {
         char inchar = KB().updateKeypress();
         // HANDLE INPUTS
         // No char recieved
         if (inchar == 0)
           ;
         // HOME Recieved
         else if (inchar == 12) {
           HOME_INIT();
         }
         // CR Recieved
         else if (inchar == 13) {
           commandSelectMonth(currentLine);
           currentLine = "";
         }
         // SHIFT Recieved
         else if (inchar == 17) {
           if (KB().getKeyboardState() == SHIFT || KB().getKeyboardState() == FN_SHIFT) {
             KB().setKeyboardState(NORMAL);
           } else if (KB().getKeyboardState() == FUNC) {
             KB().setKeyboardState(FN_SHIFT);
           } else {
             KB().setKeyboardState(SHIFT);
           }
         }
         // FN Recieved
         else if (inchar == 18) {
           if (KB().getKeyboardState() == FUNC || KB().getKeyboardState() == FN_SHIFT) {
             KB().setKeyboardState(NORMAL);
           } else if (KB().getKeyboardState() == SHIFT) {
             KB().setKeyboardState(FN_SHIFT);
           } else {
             KB().setKeyboardState(FUNC);
           }
         }
         // Space Recieved
         else if (inchar == 32) {
           currentLine += " ";
         }
         // BKSP Recieved
         else if (inchar == 8) {
           if (currentLine.length() > 0) {
             currentLine.remove(currentLine.length() - 1);
           }
         }
         // LEFT Recieved
         else if (inchar == 19) {
           monthOffsetCount--;
           newState = true;
         }
         // RIGHT Recieved
         else if (inchar == 21) {
           monthOffsetCount++;
           newState = true;
         }
         // CENTER Recieved
         else if (inchar == 20 || inchar == 7) {
           CurrentCalendarState = WEEK;
           KB().setKeyboardState(NORMAL);
           newState = true;
           delay(200);
           break;
         } else {
           currentLine += inchar;
           if (inchar >= 48 && inchar <= 57) {
           }  // Only leave FN on if typing numbers
           else if (KB().getKeyboardState() != NORMAL) {
             KB().setKeyboardState(NORMAL);
           }
         }

         currentMillis = millis();
         // Make sure oled only updates at OLED_MAX_FPS
         if (currentMillis - OLEDFPSMillis >= (1000 / OLED_MAX_FPS)) {
           OLEDFPSMillis = currentMillis;
           OLED().oledLine(currentLine, false);
         }
       }
       break;
     case WEEK:
       if (currentMillis - KBBounceMillis >= KB_COOLDOWN) {
         char inchar = KB().updateKeypress();
         // HANDLE INPUTS
         // No char recieved
         if (inchar == 0)
           ;
         // HOME Recieved
         else if (inchar == 12) {
           HOME_INIT();
         }
         // CR Recieved
         else if (inchar == 13) {
           // commandSelectMonth(currentLine);
           commandSelectWeek(currentLine);
           currentLine = "";
         }
         // SHIFT Recieved
         else if (inchar == 17) {
           if (KB().getKeyboardState() == SHIFT)
             KB().setKeyboardState(NORMAL);
           else
             KB().setKeyboardState(SHIFT);
         }
         // FN Recieved
         else if (inchar == 18) {
           if (KB().getKeyboardState() == FUNC)
             KB().setKeyboardState(NORMAL);
           else
             KB().setKeyboardState(FUNC);
         }
         // Space Recieved
         else if (inchar == 32) {
           currentLine += " ";
         }
         // BKSP Recieved
         else if (inchar == 8) {
           if (currentLine.length() > 0) {
             currentLine.remove(currentLine.length() - 1);
           }
         }
         // LEFT Recieved
         else if (inchar == 19) {
           weekOffsetCount--;
           newState = true;
         }
         // RIGHT Recieved
         else if (inchar == 21) {
           weekOffsetCount++;
           newState = true;
         }
         // CENTER Recieved
         else if (inchar == 20 || inchar == 7) {
           CurrentCalendarState = MONTH;
           KB().setKeyboardState(NORMAL);
           newState = true;
           delay(200);
           break;
         } else {
           currentLine += inchar;
           if (inchar >= 48 && inchar <= 57) {
           }  // Only leave FN on if typing numbers
           else if (KB().getKeyboardState() != NORMAL) {
             KB().setKeyboardState(NORMAL);
           }
         }

         currentMillis = millis();
         // Make sure oled only updates at OLED_MAX_FPS
         if (currentMillis - OLEDFPSMillis >= (1000 / OLED_MAX_FPS)) {
           OLEDFPSMillis = currentMillis;
           OLED().oledLine(currentLine, false);
         }
       }
       break;
     case NEW_EVENT:
       if (currentMillis - KBBounceMillis >= KB_COOLDOWN) {
         char inchar = KB().updateKeypress();
         // HANDLE INPUTS
         // No char recieved
         if (inchar == 0)
           ;
         // HOME Recieved
         else if (inchar == 12) {
           newEventState--;
           currentLine = "";
           if (newEventState < 0) {
             CurrentCalendarState = MONTH;
             currentLine = "";
             newState = true;
             KB().setKeyboardState(NORMAL);
           }
         }
         // CR Recieved
         else if (inchar == 13) {
           switch (newEventState) {
             case 0:
               // Event Name: must be non-empty
               if (currentLine.length() > 0) {
                 newEventName = currentLine;
                 newEventState++;
                 currentLine = newEventStartDate;
               } else {
                 OLED().oledWord("Error: Empty event name");
                 delay(2000);
                 currentLine = "";
               }
               break;

             case 1:
               // Start Date: must be YYYYMMDD (8-digit number)
               if (currentLine.length() == 8 && currentLine.toInt() > 10000000) {
                 newEventStartDate = currentLine;
                 newEventState++;
                 currentLine = "";
               } else {
                 OLED().oledWord("Error: Invalid date (YYYYMMDD)");
                 delay(2000);
                 currentLine = "";
               }
               break;

             case 2:
               // Start Time: must be HH:MM
               if (currentLine.length() == 5 && currentLine.charAt(2) == ':' &&
                   isDigit(currentLine.charAt(0)) && isDigit(currentLine.charAt(1)) &&
                   isDigit(currentLine.charAt(3)) && isDigit(currentLine.charAt(4))) {
                 newEventStartTime = currentLine;
                 newEventState++;
                 currentLine = "";
               } else {
                 OLED().oledWord("Error: Invalid time (HH:MM)");
                 delay(2000);
                 currentLine = "";
               }
               break;

             case 3:
               // Duration: must be H:MM or HH:MM
               {
                 int colonIdx = currentLine.indexOf(':');
                 if ((colonIdx == 1 || colonIdx == 2) && isDigit(currentLine.charAt(0)) &&
                     isDigit(currentLine.charAt(colonIdx + 1)) &&
                     isDigit(currentLine.charAt(colonIdx + 2))) {
                   newEventDuration = currentLine;
                   newEventState++;
                   currentLine = "";
                 } else {
                   OLED().oledWord("Error: Invalid duration (H:MM)");
                   delay(2000);
                   currentLine = "";
                 }
               }
               break;

             case 4:
               // Repeat: must be NO, DAILY, WEEKLY xx, MONTHLY xx, or YEARLY xx
               {
                 String code = currentLine;
                 code.toUpperCase();
                 if (code == "HELP") {
                   // Display help screen here
                   OLED().oledWord("Help screen coming soon!");
                   delay(5000);
                   currentLine = "";
                 } else if (code == "NO" || code == "DAILY" || code.startsWith("WEEKLY ") ||
                            code.startsWith("MONTHLY ") || code.startsWith("YEARLY ")) {
                   newEventRepeat = code;
                   newEventState++;
                   currentLine = "";
                 } else {
                   OLED().oledWord("Error: Invalid repeat value");
                   delay(2000);
                   currentLine = "";
                 }
               }
               break;

             case 5:
               // Note: no restrictions
               newEventNote = currentLine;
               newEventState++;
               currentLine = "";
               break;
           }

           if (newEventState > 5) {
             // Create Event
             addEvent(newEventName, newEventStartDate, newEventStartTime, newEventDuration,
                      newEventRepeat, newEventNote);
             // Return to app
             OLED().oledWord("New Event \"" + newEventName + "\" Created");
             delay(2000);
             CurrentCalendarState = MONTH;
             KB().setKeyboardState(NORMAL);
           }
           newState = true;
         }
         // SHIFT Recieved
         else if (inchar == 17) {
           if (KB().getKeyboardState() == SHIFT)
             KB().setKeyboardState(NORMAL);
           else
             KB().setKeyboardState(SHIFT);
         }
         // FN Recieved
         else if (inchar == 18) {
           if (KB().getKeyboardState() == FUNC)
             KB().setKeyboardState(NORMAL);
           else
             KB().setKeyboardState(FUNC);
         }
         // Space Recieved
         else if (inchar == 32) {
           currentLine += " ";
         }
         // BKSP Recieved
         else if (inchar == 8) {
           if (currentLine.length() > 0) {
             currentLine.remove(currentLine.length() - 1);
           }
         } else {
           currentLine += inchar;
           if (inchar >= 48 && inchar <= 57) {
           }  // Only leave FN on if typing numbers
           else if (KB().getKeyboardState() != NORMAL) {
             KB().setKeyboardState(NORMAL);
           }
         }

         currentMillis = millis();
         // Make sure oled only updates at OLED_MAX_FPS
         if (currentMillis - OLEDFPSMillis >= (1000 / OLED_MAX_FPS)) {
           OLEDFPSMillis = currentMillis;
           switch (newEventState) {
             case 0:
               OLED().oledLine(currentLine, false, "Enter the Event Name");
               break;
             case 1:
               OLED().oledLine(currentLine, false, "Enter the Start Date (YYYYMMDD)");
               break;
             case 2:
               OLED().oledLine(currentLine, false, "Enter the Start Time (HH:MM)");
               break;
             case 3:
               OLED().oledLine(currentLine, false, "Enter the Event Duration (HH:MM)");
               break;
             case 4:
               OLED().oledLine(currentLine, false, "Enter the Repeat Code or \"Help\"");
               break;
             case 5:
               OLED().oledLine(currentLine, false, "Attach a Note to the Event");
               break;
           }
         }
       }
       break;
     case VIEW_EVENT:
       if (currentMillis - KBBounceMillis >= KB_COOLDOWN) {
         char inchar = KB().updateKeypress();
         // HANDLE INPUTS
         // No char recieved
         if (inchar == 0)
           ;
         // HOME Recieved
         else if (inchar == 12) {
           CurrentCalendarState = MONTH;
           currentLine = "";
           newState = true;
           KB().setKeyboardState(NORMAL);
         }
         // CR Recieved
         else if (inchar == 13) {
           switch (newEventState) {
             case -1:
               if (currentLine == "1") {
                 newEventState = 0;
               } else if (currentLine == "2") {
                 newEventState = 1;
               } else if (currentLine == "3") {
                 newEventState = 2;
               } else if (currentLine == "4") {
                 newEventState = 3;
               } else if (currentLine == "5") {
                 newEventState = 4;
               } else if (currentLine == "6") {
                 newEventState = 5;
               } else if (currentLine == "d" || currentLine == "D") {
                 deleteEventByIndex(editingEventIndex);
                 updateEventsFile();
                 OLED().oledWord("Event : \"" + newEventName + "\" Deleted");
                 delay(2000);
                 CurrentCalendarState = MONTH;
                 currentLine = "";
                 newState = true;
                 KB().setKeyboardState(NORMAL);
               } else if (currentLine == "s" || currentLine == "S") {
                 updateEventByIndex(editingEventIndex);
                 updateEventsFile();
                 OLED().oledWord("Event : \"" + newEventName + "\" Edited");
                 delay(2000);
                 CurrentCalendarState = MONTH;
                 currentLine = "";
                 newState = true;
                 KB().setKeyboardState(NORMAL);
               }
               currentLine = "";
               break;
             case 0:
               // Event Name: must be non-empty
               if (currentLine.length() > 0) {
                 newEventName = currentLine;
                 currentLine = "";
                 newEventState = -1;
               } else {
                 OLED().oledWord("Error: Empty event name");
                 delay(2000);
                 currentLine = "";
               }
               break;

             case 1:
               // Start Date: must be YYYYMMDD (8-digit number)
               if (currentLine.length() == 8 && currentLine.toInt() > 10000000) {
                 newEventStartDate = currentLine;
                 currentLine = "";
                 newEventState = -1;
               } else {
                 OLED().oledWord("Error: Invalid date (YYYYMMDD)");
                 delay(2000);
                 currentLine = "";
               }
               break;

             case 2:
               // Start Time: must be HH:MM
               if (currentLine.length() == 5 && currentLine.charAt(2) == ':' &&
                   isDigit(currentLine.charAt(0)) && isDigit(currentLine.charAt(1)) &&
                   isDigit(currentLine.charAt(3)) && isDigit(currentLine.charAt(4))) {
                 newEventStartTime = currentLine;
                 currentLine = "";
                 newEventState = -1;
               } else {
                 OLED().oledWord("Error: Invalid time (HH:MM)");
                 delay(2000);
                 currentLine = "";
               }
               break;

             case 3:
               // Duration: must be H:MM or HH:MM
               {
                 int colonIdx = currentLine.indexOf(':');
                 if ((colonIdx == 1 || colonIdx == 2) && isDigit(currentLine.charAt(0)) &&
                     isDigit(currentLine.charAt(colonIdx + 1)) &&
                     isDigit(currentLine.charAt(colonIdx + 2))) {
                   newEventDuration = currentLine;
                   currentLine = "";
                   newEventState = -1;
                 } else {
                   OLED().oledWord("Error: Invalid duration (H:MM)");
                   delay(2000);
                   currentLine = "";
                 }
               }
               break;

             case 4:
               // Repeat: must be NO, DAILY, WEEKLY xx, MONTHLY xx, or YEARLY xx
               {
                 String code = currentLine;
                 code.toUpperCase();
                 if (code == "HELP") {
                   // Display help screen here
                   OLED().oledWord("Help screen coming soon!");
                   delay(5000);
                   currentLine = "";
                 } else if (code == "NO" || code == "DAILY" || code.startsWith("WEEKLY ") ||
                            code.startsWith("MONTHLY ") || code.startsWith("YEARLY ")) {
                   newEventRepeat = code;
                   currentLine = "";
                   newEventState = -1;
                 } else {
                   OLED().oledWord("Error: Invalid repeat value");
                   delay(2000);
                   currentLine = "";
                 }
               }
               break;

             case 5:
               // Note: no restrictions
               newEventNote = currentLine;
               currentLine = "";
               newEventState = -1;
               break;
           }

           if (newEventState > 5) {
             // Create Event
             addEvent(newEventName, newEventStartDate, newEventStartTime, newEventDuration,
                      newEventRepeat, newEventNote);
             // Return to app
             OLED().oledWord("New Event \"" + newEventName + "\" Created");
             delay(2000);
             CurrentCalendarState = MONTH;
             KB().setKeyboardState(NORMAL);
           }
           newState = true;
         }
         // SHIFT Recieved
         else if (inchar == 17) {
           if (KB().getKeyboardState() == SHIFT)
             KB().setKeyboardState(NORMAL);
           else
             KB().setKeyboardState(SHIFT);
         }
         // FN Recieved
         else if (inchar == 18) {
           if (KB().getKeyboardState() == FUNC)
             KB().setKeyboardState(NORMAL);
           else
             KB().setKeyboardState(FUNC);
         }
         // Space Recieved
         else if (inchar == 32) {
           currentLine += " ";
         }
         // BKSP Recieved
         else if (inchar == 8) {
           if (currentLine.length() > 0) {
             currentLine.remove(currentLine.length() - 1);
           }
         } else {
           currentLine += inchar;
           if (inchar >= 48 && inchar <= 57) {
           }  // Only leave FN on if typing numbers
           else if (KB().getKeyboardState() != NORMAL) {
             KB().setKeyboardState(NORMAL);
           }
         }

         currentMillis = millis();
         // Make sure oled only updates at OLED_MAX_FPS
         if (currentMillis - OLEDFPSMillis >= (1000 / OLED_MAX_FPS)) {
           OLEDFPSMillis = currentMillis;
           switch (newEventState) {
             case -1:
               OLED().oledLine(currentLine, false);
               break;
             case 0:
               OLED().oledLine(currentLine, false, "Enter the Event Name");
               break;
             case 1:
               OLED().oledLine(currentLine, false, "Enter the Start Date (YYYYMMDD)");
               break;
             case 2:
               OLED().oledLine(currentLine, false, "Enter the Start Time (HH:MM)");
               break;
             case 3:
               OLED().oledLine(currentLine, false, "Enter the Event Duration (HH:MM)");
               break;
             case 4:
               OLED().oledLine(currentLine, false, "Enter the Repeat Code or \"Help\"");
               break;
             case 5:
               OLED().oledLine(currentLine, false, "Attach a Note to the Event");
               break;
           }
         }
       }
       break;
     case SUN:
     case MON:
     case TUE:
     case WED:
     case THU:
     case FRI:
     case SAT:
       if (currentMillis - KBBounceMillis >= KB_COOLDOWN) {
         char inchar = KB().updateKeypress();
         // HANDLE INPUTS
         // No char recieved
         if (inchar == 0)
           ;
         // HOME Recieved
         else if (inchar == 12) {
           CurrentCalendarState = MONTH;
           currentLine = "";
           newState = true;
           KB().setKeyboardState(NORMAL);
         }
         // CR Recieved
         else if (inchar == 13) {
           commandSelectDay(currentLine);
           currentLine = "";
         }
         // SHIFT Recieved
         else if (inchar == 17) {
           if (KB().getKeyboardState() == SHIFT)
             KB().setKeyboardState(NORMAL);
           else
             KB().setKeyboardState(SHIFT);
         }
         // FN Recieved
         else if (inchar == 18) {
           if (KB().getKeyboardState() == FUNC)
             KB().setKeyboardState(NORMAL);
           else
             KB().setKeyboardState(FUNC);
         }
         // Space Recieved
         else if (inchar == 32) {
           currentLine += " ";
         }
         // BKSP Recieved
         else if (inchar == 8) {
           if (currentLine.length() > 0) {
             currentLine.remove(currentLine.length() - 1);
           }
         }

         // LEFT Received
         else if (inchar == 19) {
           // Go back one day
           currentDate--;
           if (currentDate < 1) {
             currentMonth--;
             if (currentMonth < 1) {
               currentMonth = 12;
               currentYear--;
             }
             currentDate = daysInMonth(currentMonth, currentYear);
           }

           int dayOfWeek = getDayOfWeek(currentYear, currentMonth, currentDate);
           switch (dayOfWeek) {
             case 0:
               CurrentCalendarState = SUN;
               break;
             case 1:
               CurrentCalendarState = MON;
               break;
             case 2:
               CurrentCalendarState = TUE;
               break;
             case 3:
               CurrentCalendarState = WED;
               break;
             case 4:
               CurrentCalendarState = THU;
               break;
             case 5:
               CurrentCalendarState = FRI;
               break;
             case 6:
               CurrentCalendarState = SAT;
               break;
           }

           newState = true;
         }

         // RIGHT Received
         else if (inchar == 21) {
           // Go forward one day
           int daysThisMonth = daysInMonth(currentMonth, currentYear);
           currentDate++;
           if (currentDate > daysThisMonth) {
             currentDate = 1;
             currentMonth++;
             if (currentMonth > 12) {
               currentMonth = 1;
               currentYear++;
             }
           }

           int dayOfWeek = getDayOfWeek(currentYear, currentMonth, currentDate);
           switch (dayOfWeek) {
             case 0:
               CurrentCalendarState = SUN;
               break;
             case 1:
               CurrentCalendarState = MON;
               break;
             case 2:
               CurrentCalendarState = TUE;
               break;
             case 3:
               CurrentCalendarState = WED;
               break;
             case 4:
               CurrentCalendarState = THU;
               break;
             case 5:
               CurrentCalendarState = FRI;
               break;
             case 6:
               CurrentCalendarState = SAT;
               break;
           }

           newState = true;
         }

         // CENTER Recieved
         else if (inchar == 20 || inchar == 7) {
           CurrentCalendarState = WEEK;
           KB().setKeyboardState(NORMAL);
           newState = true;
           delay(200);
           break;
         } else {
           currentLine += inchar;
           if (inchar >= 48 && inchar <= 57) {
           }  // Only leave FN on if typing numbers
           else if (KB().getKeyboardState() != NORMAL) {
             KB().setKeyboardState(NORMAL);
           }
         }

         currentMillis = millis();
         // Make sure oled only updates at OLED_MAX_FPS
         if (currentMillis - OLEDFPSMillis >= (1000 / OLED_MAX_FPS)) {
           OLEDFPSMillis = currentMillis;
           OLED().oledLine(currentLine, false);
         }
       }
       break;
   }
}

void einkHandler_CALENDAR() {
  switch (CurrentCalendarState) {
    case WEEK:
      if (newState) {
        newState = false;
        EINK().resetDisplay();

        // DRAW APP
        drawCalendarWeek(weekOffsetCount);

        EINK().forceSlowFullUpdate(true);
        EINK().refresh();
        //EINK().multiPassRefresh(2);
      }
      break;
    case MONTH:
      if (newState) {
        newState = false;
        EINK().resetDisplay();

        // DRAW APP
        drawCalendarMonth(monthOffsetCount);

        EINK().forceSlowFullUpdate(true);
        EINK().refresh();
        //EINK().multiPassRefresh(2);
      }
      break;
    case NEW_EVENT:
      if (newState) {
        newState = false;
        EINK().resetDisplay();

        display.drawBitmap(0, 0, calendar_allArray[2], 320, 218, GxEPD_BLACK);

        display.setFont(&FreeSerif9pt7b);

        display.setCursor(106, 68);
        display.print(newEventName);

        display.setCursor(106, 90);
        display.print(newEventStartDate);

        display.setCursor(106, 112);
        display.print(newEventStartTime);

        display.setCursor(106, 134);
        display.print(newEventDuration);
        
        display.setCursor(106, 156);
        display.print(newEventRepeat);

        display.setCursor(106, 178);
        display.print(newEventNote);

        EINK().forceSlowFullUpdate(true);
        EINK().refresh();
      }
      break;
    case VIEW_EVENT:
      if (newState) {
        newState = false;
        EINK().resetDisplay();

        switch(newEventState) {
          case -1:
            EINK().drawStatusBar("Type 1-6,(D)elete,or (S)ave");
            break;
          default:
            EINK().drawStatusBar("Type the info!");
            break;
        }
        display.drawBitmap(0, 0, calendar_allArray[3], 320, 218, GxEPD_BLACK);

        display.setFont(&FreeSerif9pt7b);

        display.setCursor(106, 68);
        display.print(newEventName);

        display.setCursor(106, 90);
        display.print(newEventStartDate);

        display.setCursor(106, 112);
        display.print(newEventStartTime);

        display.setCursor(106, 134);
        display.print(newEventDuration);
        
        display.setCursor(106, 156);
        display.print(newEventRepeat);

        display.setCursor(106, 178);
        display.print(newEventNote);

        EINK().forceSlowFullUpdate(true);
        EINK().refresh();
      }
      break;
    // All days use the same basic code
    case SUN:
    case MON:
    case TUE:
    case WED:
    case THU:
    case FRI:
    case SAT:
      if (newState) {
        newState = false;
        EINK().resetDisplay();

        // Draw background
        // CurrentCalendarState enumerations somehow line up with calendar app bitmaps?
        // SUN = 4, SAT = 10
        EINK().drawStatusBar("Events 1-7 or (N)ew");
        display.drawBitmap(0, 0, calendar_allArray[CurrentCalendarState], 320, 218, GxEPD_BLACK);

        // Draw Date
        display.setFont(&FreeSerif9pt7b);
        display.setTextColor(GxEPD_BLACK);
        // Set cursor based on the day of the week
        display.setCursor(9 + (44*(CurrentCalendarState - 4)), 59);
        display.print(String(currentMonth) + "/" + String(currentDate));

        // Load events
        String YYYYMMDD = intToYYYYMMDD(currentYear, currentMonth, currentDate);
        int eventCount = checkEvents(YYYYMMDD, false);
        if (eventCount > 7) eventCount = 7;

        // Blank out extra space
        display.fillRect(12, 66 + (eventCount * 19), 297, ((7 - eventCount) * 19), GxEPD_WHITE);
        
        // Display events data
        for (int j = 0; j < eventCount; j++) {
          String name       = dayEvents[j][0];
          String startTime  = dayEvents[j][2];
          String duration   = dayEvents[j][3];
          String repeatCode = dayEvents[j][4];
          String bottomInfo = "Starts: " + startTime + ", Dur: " + duration + ", Rep: " + repeatCode;

          // Print event name
          display.setFont(&Font5x7Fixed);
          display.setCursor(48, 74 + (j * 19));
          display.print(name);

          // Print bottom info
          display.setFont(&Font5x7Fixed);
          display.setCursor(48, 82 + (j * 19));
          display.print(bottomInfo);
        }

        EINK().forceSlowFullUpdate(true);
        EINK().refresh();
        //EINK().multiPassRefresh(2);
      }
      break;
  }
}
#endif

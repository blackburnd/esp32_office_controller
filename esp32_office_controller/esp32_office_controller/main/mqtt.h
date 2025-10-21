void weather_fetch_and_display(void) {
    // Existing code...

    // Fetch daily forecast
    char daily_url[256];
    snprintf(daily_url, sizeof(daily_url), "https://api.openweathermap.org/data/2.5/weather?lat=%s&lon=%s&appid=%s&units=imperial", WEATHER_LAT, WEATHER_LON, OPENWEATHERKEY);
    
    // Fetch weekly forecast
    char weekly_url[256];
    snprintf(weekly_url, sizeof(weekly_url), "https://api.openweathermap.org/data/2.5/onecall?lat=%s&lon=%s&exclude=current,minutely,hourly,alerts&appid=%s&units=imperial", WEATHER_LAT, WEATHER_LON, OPENWEATHERKEY);

    // Make HTTP requests for both URLs and parse the JSON responses
    // Update the UI with the fetched data
}
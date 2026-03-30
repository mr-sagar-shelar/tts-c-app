# tts-c-app

tts application based on C language to load when Tinycore linux loads on raspberry pi zero

## How to build

`make`

## Run App

`./sai`

## Next Features

- Some menus should be visible only to selected languages from the list
- Typing Tutor
  - Expect specific character from user and maintain it's accuracy, time spent and other similar statics and show at the end
- Setting Menu Completion
  - Set Time And Date
    - Time Zone: Selection from menu
    - Set Time Format: 12 or 24 hours
    - Time: Sync from internet or manual
    - Date Format: Long Short
    - Date: Set date, month, year
  - Wifi Setup
    - List available Wifi
    - Select Wifi and enter detail
  - Backup/Sync with Server
    - Save user JSON settings to server
- Multi key press for special operations
- Bard
  - Convert project to use it in this project
  - Use epub, (x) html reading logic
  - use keyboard input and selection logic
- Poem: Save locally option
- Books (Txt, Docx, PDF, Excel, Daisy), ePUB
- Book-reader – read your favorite books and files in DAISY, DOCX, PDF and ePUB formats
- Note-taker – take notes and edit files in text and braille
- Onboard Apps: Clock and Alarm, Calendar, Address book, Calculator, File Manager
- Entertainment: Media Player, Internet Radio, Podcatcher, Voice Recorder
- Library Access: Bookshare, NFB-Newsline® and many others
- Internet Radio
- FM Radio
- Messaging
- Email
- Music Player
- Phone Caller App
- Chat App
- Calendar
- Contacts
- Social Network ( Facebook, Linked In)
- Voice Recorder
- Guitar Tuning
- Podcasts
- Backup user settings
- Wifi Setup
- Internet Setup
- Voice call, SMS Setup
- Voice based interaction
- Local bus, traine schedule
- Integration with cab booking service like Uber, Bharat Taxi

## Reference Devices

- Braille Sense 6
- BrailleSense Polaris
- Device Walkthrough to start when device boot up and resume everytime user restart

## Server Platform

- Localhost setup for each school with their own dashboard and KPI running on raspberry pi 5
- Setup Server Builder so that any school can use it to create their server and download their config to run on their local setup
- AI API for natural langauge interaction
- LLM Setup on their own server to fetch their own data
- Tracking API requests and generate user profile data for better responess
- App Store implementation based on JSON API.
- Games creating using drag/drop and save it as JSON file
- Data analytics: Continuous data capture of the children's usage pattern helps the teachers and parents track device usage and each child's progress
- Interactive content: Custom-designed interactive audio-tactile content provides higher engagement and better consumption of lessons. Games, challenges, and learning assessment tools make practice fun and evaluation easy. Vernacular language medium content provides that all children learn equally well
- Multilingual Content: Available in English (Grade 1 and 2) and 10 Indian languages: Hindi, Marathi, Odia, Tamil, Kannada, Telugu, Gujarati, Malayalam, Bengali, Assamese
  Performance Tracking: Integrated with Helios dashboard that allows teachers and parents to monitor usage and progress
  Audio-Guided Navigation: Enables self-paced learning with minimal external assistance

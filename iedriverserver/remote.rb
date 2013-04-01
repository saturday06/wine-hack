require 'selenium-webdriver'

caps = Selenium::WebDriver::Remote::Capabilities.internet_explorer
caps['enablePersistentHover'] = false
driver = Selenium::WebDriver.for(:remote,
  :desired_capabilities => caps,
  :url => 'http://localhost:5555')
driver.navigate.to "http://google.com"
driver.quit


task :default => :test

task :test do
  config_path = File.expand_path('../test_conf.rb', __FILE__)
  mruby_path = ENV['MRUBY_PATH']
  puts "Using mruby from #{mruby_path}."
  Dir.chdir( mruby_path ) do
    sh "MRUBY_CONFIG=#{config_path} rake"
  end
  
  puts ""
  
  sh "./build/bin/mruby test.rb"
end

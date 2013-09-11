MRuby::Build.new do |conf|
  # load specific toolchain settings
  toolchain :gcc

  # include the default GEMs
  # conf.gembox 'default'
  
  conf.gem core: 'mruby-bin-mruby'
  conf.gem core: 'mruby-struct'
  
  conf.gem core: 'mruby-print'
  
  conf.gem          File.expand_path('../', __FILE__)
  conf.build_dir =  File.expand_path('../build', __FILE__)
    
  conf.cc do |cc|
    # cc.defines << %w(MRB_INT64)
    cc.flags = %w(-g -Wall -Werror-implicit-function-declaration)
  end

end

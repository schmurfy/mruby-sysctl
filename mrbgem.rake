MRuby::Gem::Specification.new('mruby-sysctl') do |spec|
  spec.license = 'MIT'
  spec.authors = 'Julien Ammous'
  
  # spec.linker.libraries << %w(net pcap pthread)
  
  spec.add_dependency 'mruby-struct'
end

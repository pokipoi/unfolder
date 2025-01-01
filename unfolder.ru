require 'fileutils'

dir = ARGV[0].gsub("\\", "/")

parent_dir = File.dirname(dir)
items = Dir.entries(dir) - %w[. ..]

items.each do |item|
  src = File.join(dir, item)
  dest = File.join(parent_dir, item)
  FileUtils.mv(src, dest)
end

FileUtils.rm_rf(dir)
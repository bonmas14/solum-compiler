vim.api.nvim_create_autocmd({"BufRead", "BufNewFile"}, {
  pattern = "*.slm",
  callback = function() 
    vim.bo.filetype = "slm"
    vim.cmd[[doautocmd FileType]]
  end
})

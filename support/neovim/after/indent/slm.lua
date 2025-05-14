vim.api.nvim_create_autocmd("FileType", {
  pattern = "slm",
  callback = function()
    vim.bo.tabstop = 4
    vim.bo.shiftwidth = 4
    vim.bo.softtabstop = 4
    vim.bo.expandtab = true
    vim.bo.smartindent = true
  end
})
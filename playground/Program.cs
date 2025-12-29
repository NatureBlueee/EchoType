using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace FloatingTool
{
    public class FloatingToolForm : Form
    {
        // Win32 API Imports
        [DllImport("user32.dll")]
        private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

        [DllImport("user32.dll")]
        private static extern bool UnregisterHotKey(IntPtr hWnd, int id);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool GetCursorPos(out POINT lpPoint);

        [DllImport("user32.dll")]
        private static extern bool SetForegroundWindow(IntPtr hWnd);

        [StructLayout(LayoutKind.Sequential)]
        public struct POINT
        {
            public int X;
            public int Y;
        }

        private const int HOTKEY_ID = 1;
        private const uint MOD_ALT = 0x0001;
        private const uint VK_Q = 0x51;
        private const int WM_HOTKEY = 0x0312;

        private TextBox inputField;

        public FloatingToolForm()
        {
            // 窗口设置
            this.Text = "快速反馈";
            this.FormBorderStyle = FormBorderStyle.FixedToolWindow;
            this.StartPosition = FormStartPosition.Manual;
            this.Size = new Size(320, 130);
            this.BackColor = Color.FromArgb(45, 45, 48);
            this.ShowInTaskbar = false;
            this.TopMost = true;

            // 按钮样式
            Button btnLike = CreateButton("👍 喜欢", 10, 10, Color.FromArgb(76, 175, 80));
            Button btnDislike = CreateButton("👎 不喜欢", 110, 10, Color.FromArgb(244, 67, 54));
            Button btnMeh = CreateButton("😐 一般", 210, 10, Color.FromArgb(158, 158, 158));

            btnLike.Click += (s, e) => HandleReaction("喜欢");
            btnDislike.Click += (s, e) => HandleReaction("不喜欢");
            btnMeh.Click += (s, e) => HandleReaction("一般");

            // 文本输入框
            inputField = new TextBox();
            inputField.Location = new Point(10, 55);
            inputField.Size = new Size(290, 30);
            inputField.Font = new Font("Microsoft YaHei", 10);
            inputField.BackColor = Color.FromArgb(60, 60, 64);
            inputField.ForeColor = Color.White;
            inputField.BorderStyle = BorderStyle.FixedSingle;
            inputField.KeyDown += InputField_KeyDown;
            this.Controls.Add(inputField);

            // 提示标签
            Label hint = new Label();
            hint.Text = "输入文字后按 Enter 提交";
            hint.Location = new Point(10, 90);
            hint.Size = new Size(290, 20);
            hint.ForeColor = Color.Gray;
            hint.Font = new Font("Microsoft YaHei", 8);
            this.Controls.Add(hint);

            // 注册快捷键 Alt + Q
            if (!RegisterHotKey(this.Handle, HOTKEY_ID, MOD_ALT, VK_Q))
            {
                MessageBox.Show("无法注册快捷键 Alt+Q，可能被其他程序占用。", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private Button CreateButton(string text, int x, int y, Color bgColor)
        {
            Button btn = new Button();
            btn.Text = text;
            btn.Location = new Point(x, y);
            btn.Size = new Size(90, 35);
            btn.FlatStyle = FlatStyle.Flat;
            btn.FlatAppearance.BorderSize = 0;
            btn.BackColor = bgColor;
            btn.ForeColor = Color.White;
            btn.Font = new Font("Microsoft YaHei", 9);
            btn.Cursor = Cursors.Hand;
            this.Controls.Add(btn);
            return btn;
        }

        private void InputField_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                string text = inputField.Text;
                Console.WriteLine($"输入内容: {text}");
                HideWindow();
                e.Handled = true;
                e.SuppressKeyPress = true;
            }
            else if (e.KeyCode == Keys.Escape)
            {
                HideWindow();
                e.Handled = true;
            }
        }

        private void HandleReaction(string reaction)
        {
            Console.WriteLine($"反馈: {reaction}");
            HideWindow();
        }

        private void ShowAtCursor()
        {
            POINT p;
            if (GetCursorPos(out p))
            {
                // 确保窗口不会超出屏幕边缘
                Rectangle screen = Screen.FromPoint(new Point(p.X, p.Y)).WorkingArea;
                int x = Math.Min(p.X, screen.Right - this.Width);
                int y = Math.Min(p.Y, screen.Bottom - this.Height);
                
                this.Location = new Point(x, y);
                this.Show();
                SetForegroundWindow(this.Handle);
                inputField.Focus();
                inputField.Text = "";
            }
        }

        private void HideWindow()
        {
            this.Hide();
        }

        protected override void WndProc(ref Message m)
        {
            if (m.Msg == WM_HOTKEY && m.WParam.ToInt32() == HOTKEY_ID)
            {
                ShowAtCursor();
            }
            base.WndProc(ref m);
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            UnregisterHotKey(this.Handle, HOTKEY_ID);
            base.OnFormClosing(e);
        }

        protected override void SetVisibleCore(bool value)
        {
            // 启动时隐藏窗口
            if (!this.IsHandleCreated)
            {
                CreateHandle();
                value = false;
            }
            base.SetVisibleCore(value);
        }
    }

    static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new FloatingToolForm());
        }
    }
}
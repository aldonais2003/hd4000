using System;
using System.Windows.Forms;
namespace IntelHD4000ControlPanel {
    public class MainForm : Form {
        public MainForm() { this.Text = "Intel HD 4000 Control Panel"; }
        [STAThread] public static void Main() { Application.Run(new MainForm()); }
    }
}

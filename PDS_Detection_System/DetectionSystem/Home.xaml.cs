using MySql.Data.MySqlClient;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace DetectionSystem
{
    /// <summary>
    /// Logica di interazione per Home.xaml
    /// </summary>
    public partial class Home : Page
    {
        protected Process TCPServer;
        protected MySqlConnection DBconnection;

        protected string filename;
        public static Label espn_box;
        public static Label output_box;

        protected const int startEspNum = 1;
        protected int espNum;

        public Home()
        {
            InitializeComponent();
            esp_number.Content = startEspNum;
            espNum = startEspNum;
            espn_box = (Label)this.FindName("esp_number");
            output_box = (Label)this.FindName("info_picker");
            for (int i = startEspNum; i < 8; i++) {
                TextBox espBoxX = (TextBox)this.FindName("esp" + i + "x");
                espBoxX.Visibility = Visibility.Hidden;
                TextBox espBoxY = (TextBox)this.FindName("esp" + i + "y");
                espBoxY.Visibility = Visibility.Hidden;
                Label espBoxXL = (Label)this.FindName("esp" + i + "xl");
                espBoxXL.Visibility = Visibility.Hidden;
                Label espBoxYL = (Label)this.FindName("esp" + i + "yl");
                espBoxYL.Visibility = Visibility.Hidden;
            }
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            if (filename == null) {
                output_box.Content = "Error: Server executable file not selected.";
                return;
            }        
            
            string num = espn_box.Content.ToString();
            string x0 = esp0x.Text, y0 = esp0y.Text;
            string x1 = esp1x.Text, y1 = esp1y.Text;
            string x2 = esp2x.Text, y2 = esp2y.Text;
            string x3 = esp3x.Text, y3 = esp3y.Text;
            string x4 = esp4x.Text, y4 = esp4y.Text;
            string x5 = esp5x.Text, y5 = esp5y.Text;
            string x6 = esp6x.Text, y6 = esp6y.Text;
            string x7 = esp7x.Text, y7 = esp7y.Text;
            string args = num + " " + x0 + " " + y0 + " " + x1 + " " + y1 + " " +
                                                        x2 + " " + y2 + " " + x3 + " " + y3 + " " +
                                                        x4 + " " + y4 + " " + x5 + " " + y5 + " " +
                                                        x6 + " " + y6 + " " + x7 + " " + y7;
                
            DataWindow dataW = new DataWindow(args, filename);
            dataW.Show();
            Application.Current.MainWindow.Close();
        }

        private void Exe_picker_Click(object sender, RoutedEventArgs e)
        {
            Microsoft.Win32.OpenFileDialog dlg = new Microsoft.Win32.OpenFileDialog();
            dlg.Filter = "Executable File (.exe) | *.exe";
            Nullable<bool> result = dlg.ShowDialog();

            if (result == true) {
                filename = dlg.FileName; 
            }
        }

        private void Esp_increase_Click(object sender, RoutedEventArgs e)
        {
            if (espNum < 8) {
                TextBox espBoxX = (TextBox)this.FindName("esp" + espNum + "x");
                espBoxX.Visibility = Visibility.Visible;
                TextBox espBoxY = (TextBox)this.FindName("esp" + espNum + "y");
                espBoxY.Visibility = Visibility.Visible;
                Label espBoxXL = (Label)this.FindName("esp" + espNum + "xl");
                espBoxXL.Visibility = Visibility.Visible;
                Label espBoxYL = (Label)this.FindName("esp" + espNum + "yl");
                espBoxYL.Visibility = Visibility.Visible;
                espNum++;
                esp_number.Content = espNum;
            }
        }

        private void Esp_derease_Click(object sender, RoutedEventArgs e)
        {
            if (espNum > 1)
            {
                espNum--;
                esp_number.Content = espNum;
                TextBox espBoxX = (TextBox)this.FindName("esp" + espNum + "x");
                espBoxX.Visibility = Visibility.Hidden;
                TextBox espBoxY = (TextBox)this.FindName("esp" + espNum + "y");
                espBoxY.Visibility = Visibility.Hidden;
                Label espBoxXL = (Label)this.FindName("esp" + espNum + "xl");
                espBoxXL.Visibility = Visibility.Hidden;
                Label espBoxYL = (Label)this.FindName("esp" + espNum + "yl");
                espBoxYL.Visibility = Visibility.Hidden;                
            }
        }
    }
}

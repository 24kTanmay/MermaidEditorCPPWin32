using System.Windows;

namespace Mermaid
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            // Additional startup logic can be added here
        }

        protected override void OnExit(ExitEventArgs e)
        {
            base.OnExit(e);
            // Cleanup logic can be added here
        }
    }
}
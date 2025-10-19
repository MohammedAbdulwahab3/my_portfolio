import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Card } from "@/components/ui/card";
import { Alert, AlertDescription } from "@/components/ui/alert";
import { useAuth } from "@/contexts/AuthContext";

const TemporaryBypass = () => {
  const [bypassEnabled, setBypassEnabled] = useState(false);
  const { user } = useAuth();

  const enableBypass = () => {
    // Store bypass in localStorage (temporary solution)
    localStorage.setItem('admin-bypass', user?.id || '');
    localStorage.setItem('admin-bypass-timestamp', Date.now().toString());
    setBypassEnabled(true);
    
    // Refresh page to apply bypass
    setTimeout(() => window.location.reload(), 1000);
  };

  return (
    <Card className="p-6">
      <h3 className="text-lg font-semibold mb-4">Temporary Admin Bypass</h3>
      
      <Alert className="mb-4">
        <AlertDescription>
          This is a temporary solution to bypass admin role requirements. 
          Use this only for development/testing purposes.
        </AlertDescription>
      </Alert>

      <div className="space-y-4">
        <p className="text-sm text-muted-foreground">
          Current user: {user?.email}
        </p>
        
        <Button 
          onClick={enableBypass}
          disabled={bypassEnabled}
          variant="secondary"
          className="w-full"
        >
          {bypassEnabled ? "Bypass Enabled" : "Enable Admin Bypass"}
        </Button>

        <div className="text-xs text-muted-foreground">
          <p><strong>Note:</strong> This bypass only works in your browser and is temporary.</p>
          <p>For production, you'll need proper admin role setup.</p>
        </div>
      </div>
    </Card>
  );
};

export default TemporaryBypass;

import { useEffect, useState } from "react";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { Label } from "@/components/ui/label";
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogTrigger } from "@/components/ui/dialog";
import { supabase } from "@/integrations/supabase/client";
import { Trash2, Eye, Archive, Reply, Send } from "lucide-react";
import { toast } from "sonner";

interface Contact {
  id: string;
  name: string;
  email: string;
  message: string;
  status: string;
  created_at: string;
}

const ContactsManagement = () => {
  const [contacts, setContacts] = useState<Contact[]>([]);
  const [loading, setLoading] = useState(true);
  const [replyDialogOpen, setReplyDialogOpen] = useState(false);
  const [selectedContact, setSelectedContact] = useState<Contact | null>(null);
  const [replyForm, setReplyForm] = useState({
    subject: "",
    message: "",
  });
  const [sending, setSending] = useState(false);

  const fetchContacts = async () => {
    setLoading(true);
    const { data, error } = await supabase
      .from("contacts")
      .select("*")
      .order("created_at", { ascending: false });

    if (error) {
      toast.error("Failed to load contacts");
    } else {
      setContacts(data || []);
    }
    setLoading(false);
  };

  useEffect(() => {
    fetchContacts();
  }, []);

  const updateStatus = async (id: string, status: string) => {
    const { error } = await supabase
      .from("contacts")
      .update({ status })
      .eq("id", id);

    if (error) {
      toast.error("Failed to update status");
    } else {
      fetchContacts();
      toast.success("Contact status updated");
    }
  };

  const deleteContact = async (id: string) => {
    const { error } = await supabase.from("contacts").delete().eq("id", id);

    if (error) {
      toast.error("Failed to delete contact");
    } else {
      fetchContacts();
      toast.success("Contact deleted");
    }
  };

  const handleReply = (contact: Contact) => {
    setSelectedContact(contact);
    setReplyForm({
      subject: `Re: Contact from ${contact.name}`,
      message: `Hi ${contact.name},\n\nThank you for reaching out. `,
    });
    setReplyDialogOpen(true);
  };

  const sendReply = async () => {
    if (!selectedContact || !replyForm.subject.trim() || !replyForm.message.trim()) {
      toast.error("Please fill in all fields");
      return;
    }

    setSending(true);
    try {
      // In a real application, you would integrate with an email service like:
      // - SendGrid
      // - Mailgun
      // - AWS SES
      // - Resend
      
      // For now, we'll simulate sending and update the contact status
      await updateStatus(selectedContact.id, "replied");
      
      // Simulate email sending delay
      await new Promise(resolve => setTimeout(resolve, 1000));
      
      toast.success(`Reply sent to ${selectedContact.email}`);
      setReplyDialogOpen(false);
      setReplyForm({ subject: "", message: "" });
      setSelectedContact(null);
      
      // In a real app, you would make an API call like:
      // const response = await fetch('/api/send-email', {
      //   method: 'POST',
      //   headers: { 'Content-Type': 'application/json' },
      //   body: JSON.stringify({
      //     to: selectedContact.email,
      //     subject: replyForm.subject,
      //     message: replyForm.message,
      //   }),
      // });
      
    } catch (error) {
      console.error("Error sending reply:", error);
      toast.error("Failed to send reply");
    } finally {
      setSending(false);
    }
  };

  if (loading) {
    return <div className="text-center py-12">Loading contacts...</div>;
  }

  return (
    <div className="space-y-4">
      {contacts.length === 0 ? (
        <Card className="glass p-12 text-center">
          <p className="text-muted-foreground">No contacts yet</p>
        </Card>
      ) : (
        contacts.map((contact) => (
          <Card key={contact.id} className="glass p-6">
            <div className="flex justify-between items-start mb-4">
              <div>
                <h3 className="text-xl font-semibold">{contact.name}</h3>
                <p className="text-muted-foreground">{contact.email}</p>
                <p className="text-sm text-muted-foreground mt-1">
                  {new Date(contact.created_at).toLocaleDateString()}
                </p>
              </div>
              <Badge
                variant={
                  contact.status === "unread"
                    ? "default"
                    : contact.status === "read"
                    ? "secondary"
                    : "outline"
                }
              >
                {contact.status}
              </Badge>
            </div>
            <p className="text-foreground mb-4 whitespace-pre-wrap">{contact.message}</p>
            <div className="flex gap-2 flex-wrap">
              {contact.status === "unread" && (
                <Button
                  size="sm"
                  variant="outline"
                  onClick={() => updateStatus(contact.id, "read")}
                >
                  <Eye className="mr-2 h-4 w-4" />
                  Mark as Read
                </Button>
              )}
              {contact.status === "read" && (
                <Button
                  size="sm"
                  variant="outline"
                  onClick={() => updateStatus(contact.id, "archived")}
                >
                  <Archive className="mr-2 h-4 w-4" />
                  Archive
                </Button>
              )}
              <Button
                size="sm"
                variant="default"
                onClick={() => handleReply(contact)}
              >
                <Reply className="mr-2 h-4 w-4" />
                Reply
              </Button>
              <Button
                size="sm"
                variant="destructive"
                onClick={() => deleteContact(contact.id)}
              >
                <Trash2 className="mr-2 h-4 w-4" />
                Delete
              </Button>
            </div>
          </Card>
        ))
      )}

      {/* Reply Dialog */}
      <Dialog open={replyDialogOpen} onOpenChange={setReplyDialogOpen}>
        <DialogContent className="max-w-2xl">
          <DialogHeader>
            <DialogTitle>Reply to {selectedContact?.name}</DialogTitle>
          </DialogHeader>
          <div className="space-y-4">
            {selectedContact && (
              <div className="bg-muted p-4 rounded-lg">
                <p className="text-sm text-muted-foreground mb-2">Original message:</p>
                <p className="text-sm">{selectedContact.message}</p>
              </div>
            )}
            <div>
              <Label htmlFor="reply-subject">Subject</Label>
              <Input
                id="reply-subject"
                value={replyForm.subject}
                onChange={(e) => setReplyForm({ ...replyForm, subject: e.target.value })}
                placeholder="Email subject"
              />
            </div>
            <div>
              <Label htmlFor="reply-message">Message</Label>
              <Textarea
                id="reply-message"
                value={replyForm.message}
                onChange={(e) => setReplyForm({ ...replyForm, message: e.target.value })}
                placeholder="Your reply message..."
                rows={8}
              />
            </div>
            <div className="flex justify-end gap-2">
              <Button
                variant="outline"
                onClick={() => setReplyDialogOpen(false)}
                disabled={sending}
              >
                Cancel
              </Button>
              <Button onClick={sendReply} disabled={sending}>
                {sending ? (
                  <>
                    <div className="animate-spin rounded-full h-4 w-4 border-b-2 border-white mr-2"></div>
                    Sending...
                  </>
                ) : (
                  <>
                    <Send className="mr-2 h-4 w-4" />
                    Send Reply
                  </>
                )}
              </Button>
            </div>
          </div>
        </DialogContent>
      </Dialog>
    </div>
  );
};

export default ContactsManagement;
